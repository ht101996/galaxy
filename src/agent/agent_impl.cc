// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yanshiguang02@baidu.com

#include "agent_impl.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <boost/bind.hpp>
#include <errno.h>
#include <string.h>
#include "common/util.h"
#include "rpc/rpc_client.h"

extern std::string FLAGS_master_addr;
extern std::string FLAGS_agent_port;
extern std::string FLAGS_agent_work_dir;
extern int FLAGS_cpu_num;
extern int FLAGS_mem_gbytes;

namespace galaxy {

AgentImpl::AgentImpl() {
    rpc_client_ = new RpcClient();
    ws_mgr_ = new WorkspaceManager(FLAGS_agent_work_dir + "/data/");
    task_mgr_ = new TaskManager();
    if (!rpc_client_->GetStub(FLAGS_master_addr, &master_)) {
        assert(0);
    }
    thread_pool_.Start();
    thread_pool_.AddTask(boost::bind(&AgentImpl::Report, this));
}

AgentImpl::~AgentImpl() {
    delete ws_mgr_;
    delete task_mgr_;

}

bool AgentImpl::Init() {
    const int MKDIR_MODE = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
    // clear work_dir and kill tasks
    std::string dir = FLAGS_agent_work_dir + "/data";
    if (access(FLAGS_agent_work_dir.c_str(), F_OK) != 0) {
        if (mkdir(FLAGS_agent_work_dir.c_str(), MKDIR_MODE) != 0) {
            LOG(WARNING, "mkdir data failed %s err[%d: %s]", 
                    FLAGS_agent_work_dir.c_str(), errno, strerror(errno)); 
            return false;
        } 
    }
    if (access(dir.c_str(), F_OK) == 0) {
        std::string rm_cmd = "rm -rf " + dir;
        if (system(rm_cmd.c_str()) == -1) {
            LOG(WARNING, "rm data failed cmd %s err[%d: %s]", 
                    rm_cmd.c_str(), errno, strerror(errno)); 
            return false;
        }
        LOG(INFO, "clear dirty data %s by cmd[%s]", dir.c_str(), rm_cmd.c_str());
    }

    if (mkdir(dir.c_str(), MKDIR_MODE) != 0) {
        LOG(WARNING, "mkdir data failed %s err[%d: %s]", 
                dir.c_str(), errno, strerror(errno)); 
        return false;
    }
    LOG(INFO, "init workdir %s", dir.c_str());
    return true;
}

void AgentImpl::Report() {
    HeartBeatRequest request;
    HeartBeatResponse response;
    std::string addr = common::util::GetLocalHostName() + ":" + FLAGS_agent_port;
    
    std::vector<TaskStatus > status_vector;
    task_mgr_->Status(status_vector);
    std::vector<TaskStatus>::iterator it = status_vector.begin();
    for(; it != status_vector.end(); ++it){
        TaskStatus* req_status = request.add_task_status();
        req_status->set_task_id(it->task_id());
        req_status->set_status(it->status());
    }
    request.set_agent_addr(addr);
    request.set_cpu_share(FLAGS_cpu_num);
    request.set_mem_share(FLAGS_mem_gbytes);

    LOG(INFO, "Reprot to master %s,task count %d", addr.c_str(),request.task_status_size());
    rpc_client_->SendRequest(master_, &Master_Stub::HeartBeat,
                                &request, &response, 5, 1);
    thread_pool_.DelayTask(5000, boost::bind(&AgentImpl::Report, this));
}

void AgentImpl::RunTask(::google::protobuf::RpcController* /*controller*/,
                        const ::galaxy::RunTaskRequest* request,
                        ::galaxy::RunTaskResponse* response,
                        ::google::protobuf::Closure* done) {
    LOG(INFO, "Run Task %s %s", request->task_name().c_str(), request->cmd_line().c_str());
    TaskInfo task_info;
    task_info.set_task_id(request->task_id());
    task_info.set_task_name(request->task_name());
    task_info.set_cmd_line(request->cmd_line());
    task_info.set_task_raw(request->task_raw());
    LOG(INFO,"start to prepare workspace for %s",request->task_name().c_str());
    int ret = ws_mgr_->Add(task_info);
    if (ret != 0 ){
        LOG(FATAL,"fail to prepare workspace ");
        response->set_status(-2);
        done->Run();
    } else {
        LOG(INFO,"start  task for %s",request->task_name().c_str());
        DefaultWorkspace * workspace ;
        workspace = ws_mgr_->GetWorkspace(task_info);
        ret = task_mgr_->Add(task_info,workspace);
        if (ret != 0){
           LOG(FATAL,"fail to start task");
           response->set_status(-1);
        }
        response->set_status(0);
        done->Run();
    }
    //OpenProcess(request->task_name(), request->task_raw(), request->cmd_line(),"/tmp");
    //done->Run();
}
void AgentImpl::KillTask(::google::protobuf::RpcController* /*controller*/,
                         const ::galaxy::KillTaskRequest* request,
                         ::galaxy::KillTaskResponse* response,
                         ::google::protobuf::Closure* done){
    LOG(INFO,"kill task %d",request->task_id());
    int status = task_mgr_->Remove(request->task_id());
    LOG(INFO,"kill task %d status %d",request->task_id(),status);
    status = ws_mgr_->Remove(request->task_id());
    LOG(INFO,"clean workspace task  %d status %d",request->task_id(),status);
    response->set_status(status);
    done->Run();
}
} // namespace galxay

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
