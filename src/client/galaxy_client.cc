// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yanshiguang02@baidu.com

#include "sdk/galaxy.h"

#include <cstdio>
#include <cstdlib>
#include <errno.h>
#include <boost/algorithm/string/predicate.hpp>

double FLAGS_cpu_limit = 0;
int64_t FLAGS_deploy_step_size = 0;

void help() {
    fprintf(stderr, "./galaxy_client master_addr command(list/add/kill) args\n");
    fprintf(stderr, "./galaxy_client master_addr add jod_name task_raw cmd_line replicate_count cpu_quota mem_quota size cpu_limit --monitor_conf=\n");
    fprintf(stderr, "./galaxy_client master_addr list task_id\n");
    fprintf(stderr, "./galaxy_client master_addr kill task_id\n");
    return;
}

enum Command {
    LIST = 0,
    LISTJOB,
    LISTTASKBYAGENT,
    UPDATEJOB,
    LISTNODE,
    ADD,
    KILLTASK,
    KILLJOB
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        help();
        return -1;
    }
    int COMMAND = 0;
    if (strcmp(argv[2], "add") == 0) {
        COMMAND = ADD;
        if (argc < 9) {
            help();
            return -1;
        }
    } else if (strcmp(argv[2], "list") == 0) {
        COMMAND = LIST;
    } else if (strcmp(argv[2], "listjob") == 0) {
        COMMAND = LISTJOB;
    } else if (strcmp(argv[2], "listnode") == 0) {
        COMMAND = LISTNODE;
    } else if (strcmp(argv[2], "kill") == 0) {
        COMMAND = KILLTASK;
        if (argc < 4) {
            help();
            return -1;
        }
    } else if (strcmp(argv[2], "killjob") == 0) {
        COMMAND = KILLJOB;
        if (argc < 4) {
            help();
            return -1;
        }
    } else if (strcmp(argv[2], "listtaskbyagent") == 0){
        COMMAND = LISTTASKBYAGENT;
        if (argc < 4) {
           help();
           return -1;
        }

    } else if (strcmp(argv[2], "updatejob") == 0) {
        COMMAND = UPDATEJOB;
        if (argc < 5) {
            help();
            return -1;
        }
    } else {
        help();
        return -1;
    }

    if (COMMAND == ADD) {
        std::string task_raw;
        if (!boost::starts_with(argv[4], "ftp://")) {
            FILE* fp = fopen(argv[4], "r");
            if (fp == NULL) {
                fprintf(stderr, "Open %s for read fail\n", argv[4]);
                return -2;
            }
            char buf[1024];
            int len = 0;
            while ((len = fread(buf, 1, 1024, fp)) > 0) {
                task_raw.append(buf, len);
            }
            fclose(fp);
            printf("Task binary len %lu\n", task_raw.size());
        }
        else {
            task_raw = argv[4];
        }
        galaxy::Galaxy* galaxy = galaxy::Galaxy::ConnectGalaxy(argv[1]);
        galaxy::JobDescription job;
        galaxy::PackageDescription pkg;
        pkg.source = task_raw;
        job.pkg = pkg;
        job.cmd_line = argv[5];
        job.replicate_count = atoi(argv[6]);
        job.job_name = argv[3];
        job.cpu_share = atof(argv[7]);
        job.mem_share = 1024 * 1024 * 1024 * atol(argv[8]);

        for (int arg_ind = 9; arg_ind < argc; arg_ind++) {
            char temp_arg_buffer[1024];
            if (sscanf(argv[arg_ind], 
                        "--deploy_step_size=%s", 
                        temp_arg_buffer) == 1) {
                FLAGS_deploy_step_size 
                    = atol(temp_arg_buffer); 
            } else if (sscanf(argv[arg_ind],
                        "--cpu_limit=%s", 
                        temp_arg_buffer)) {
                FLAGS_cpu_limit 
                    = atof(temp_arg_buffer); 
            } else if (sscanf(argv[arg_ind],
                        "--monitor_conf=%s",
                        temp_arg_buffer)) {
                FILE* fd = fopen(temp_arg_buffer, "r");
                if (fd == NULL) {
                    fprintf(stderr, "Open %s for read fail [%d:%s]\n", temp_arg_buffer, 
                            errno, strerror(errno));
                    return -2;
                }
                std::string monitor_conf;
                char buf[1024];
                int len = 0;
                while ((len = fread(buf, 1, 1024, fd)) > 0) {
                    monitor_conf.append(buf, len);
                }
                fclose(fd);
                printf("monitor_conf len %lu\n", monitor_conf.size());
                job.monitor_conf = monitor_conf;
            }
        }
        job.deploy_step_size = FLAGS_deploy_step_size;
        job.cpu_limit = FLAGS_cpu_limit;
        fprintf(stdout,"%ld",galaxy->NewJob(job));
    } else if (COMMAND == LIST) {
        int64_t job_id = -1;
        if (argc == 4) {
            job_id = atoi(argv[3]);
        }
        int64_t task_id = -1;
        if (argc == 5) {
            task_id = atoi(argv[4]);
        }
        galaxy::Galaxy* galaxy = galaxy::Galaxy::ConnectGalaxy(argv[1]);
        galaxy->ListTask(job_id, task_id, NULL);
    } else if (COMMAND== LISTTASKBYAGENT) {
        galaxy::Galaxy* galaxy = galaxy::Galaxy::ConnectGalaxy(argv[1]);
        galaxy->ListTaskByAgent(argv[3], NULL);
    } else if (COMMAND == LISTNODE) {
        galaxy::Galaxy* galaxy = galaxy::Galaxy::ConnectGalaxy(argv[1]);
        std::vector<galaxy::NodeDescription> nodes;
        galaxy->ListNode(&nodes);
        std::vector<galaxy::NodeDescription>::iterator it = nodes.begin();
        fprintf(stdout, "================================\n");
        for(; it != nodes.end(); ++it){
            fprintf(stdout, "%ld\t%s\tTASK:%d\tCPU:%0.2f\t"
                    "USED:%0.2f\tMEM:%ldGB\tUSED:%ldGB\n",
                    it->node_id, it->addr.c_str(),
                    it->task_num, it->cpu_share,
                    it->cpu_used, it->mem_share/(1024*1024*1024),
                    it->mem_used/(1024*1024*1024));
        }
    } else if (COMMAND == LISTJOB) {
        galaxy::Galaxy* galaxy = galaxy::Galaxy::ConnectGalaxy(argv[1]);
        std::vector<galaxy::JobInstanceDescription> jobs;
        galaxy->ListJob(&jobs);
        std::vector<galaxy::JobInstanceDescription>::iterator it = jobs.begin();
        fprintf(stdout, "================================\n");
        for(;it != jobs.end();++it){
            fprintf(stdout, "%ld\t%s\t%d\t%d\n",
                    it->job_id, it->job_name.c_str(),
                    it->running_task_num, it->replicate_count);
        }
    } else if (COMMAND == KILLTASK) {
        int64_t task_id = atoi(argv[3]);
        galaxy::Galaxy* galaxy = galaxy::Galaxy::ConnectGalaxy(argv[1]);
        galaxy->KillTask(task_id);
    } else if (COMMAND == KILLJOB) {
        int64_t job_id = atoi(argv[3]);
        galaxy::Galaxy* galaxy = galaxy::Galaxy::ConnectGalaxy(argv[1]);
        galaxy->TerminateJob(job_id);
    } else if (COMMAND == UPDATEJOB) {
        galaxy::Galaxy* galaxy = galaxy::Galaxy::ConnectGalaxy(argv[1]);
        galaxy::JobDescription job;
        job.replicate_count = atoi(argv[4]);
        job.job_id  =  atoi(argv[3]);
        galaxy->UpdateJob(job);
    }
    return 0;
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
