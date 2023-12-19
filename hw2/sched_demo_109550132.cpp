#include <bits/stdc++.h>
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
#include <time.h>
using namespace std;

pthread_barrier_t bar;

typedef struct {
	int thread_id;
	double time;
	int sched_policy;
	int sched_priority;
} thread_info_t;

enum val{
    NORMAL = 0,
    FIFO = 1
};

void parse_policies (vector<string>& policies_str, vector<int>& policies) {
    for (int i = 0; i < policies_str.size(); i++) {
        if (policies_str[i] == "NORMAL")  {
            policies.push_back(0);
        }
        else if (policies_str[i]== "FIFO")  {
            policies.push_back(1);
        }
        else {
            policies.push_back(-1);
        }
    }
    return;
}
void *thread_func(void *arg)
{
    thread_info_t *cur_thread = new thread_info_t;
    memcpy(cur_thread, arg, sizeof(thread_info_t));

    if (cur_thread->sched_policy == FIFO) {
        struct sched_param param;
        param.sched_priority = cur_thread->sched_priority;
        int ret = sched_setscheduler(0, SCHED_FIFO, &param);
    }
    /* 1. Wait until all threads are ready */   
    pthread_barrier_wait(&bar);

    /* 2. Do the task */ 
    clock_t start_time , end_time;
    double run_time = 0.0;
    for (int i = 0; i < 3; i++) {
        printf("Thread %d is running\n", cur_thread->thread_id);
        /* Busy for <time_wait> seconds */
        start_time = clock();
        while (run_time < cur_thread->time) {
            end_time = clock();
            run_time = (double) (end_time - start_time) / CLOCKS_PER_SEC;
        }        
    }

    /* 3. Exit the function  */
    delete cur_thread;
    pthread_exit(NULL);
}

int main (int argc, char* argv[]) {

    int             num_threads = 0;
    float           time_wait = 0.0;
    vector<string>  policies_str;
    vector<int>     policies;
    vector<int>     priorities;


    /* 1. Parse program arguments */
    int     opt;
    while ((opt = getopt(argc, argv, "n:t:s:p:")) != -1) {
            switch (opt) {
                case 'n':
                    num_threads = stoi(optarg);
                    break;
                case 't':
                    time_wait = stof(optarg);
                    break;
                case 's':
                    {
                        char* token = strtok(optarg, ",");
                        while (token != nullptr) {
                            policies_str.push_back(token);
                            token = strtok(nullptr, ",");
                        }
                    }
                    break;
                case 'p':
                    {
                        char* token = strtok(optarg, ",");
                        while (token != nullptr) {
                            priorities.push_back(stoi(token));
                            token = strtok(nullptr, ",");
                        }
                    }
                    break;
                default:
                    cerr << "Invalid arguments. Usage: -n <num_threads> -t <time_wait> \
                    -s <policies> -p <priorities>" << endl;
                    exit(EXIT_FAILURE);
            }
        }
        if (policies_str.size() != num_threads || priorities.size() != num_threads) {
            cerr << "Invalid arguments nums. Usage: -n <num_threads> -t <time_wait> \
            -s <policies> -p <priorities>" << endl;
            exit(EXIT_FAILURE);
        }
        parse_policies(policies_str, policies);

        // for (int i = 0; i < num_threads; i++) {
        //     cout<< policies_str[i]<<" "<<policies[i] <<" "<<priorities[i] << endl;
        // }

    /* 2. Create <num_threads> worker threads */
	pthread_t threads_id[num_threads];  //create thread
    
    /* 3. Set CPU affinity */
    int     cpu_id = 0;
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(cpu_id, &cpu_set);    
	sched_setaffinity(0, sizeof(cpu_set), &cpu_set);  



    /* 4. Set the attributes to each thread */
    /* 5. Start all threads at once */

    pthread_barrier_init(&bar, NULL, num_threads);  
    thread_info_t threads_info[num_threads];

    for (int i = 0; i < num_threads; i++) {
        threads_info[i].thread_id = i;
        threads_info[i].sched_policy = policies[i];
        threads_info[i].sched_priority = priorities[i];
        threads_info[i].time = time_wait;

        int ret = pthread_create(&threads_id[i], NULL, thread_func, &threads_info[i]);
        assert(ret == 0);
    }
    

    /* 6. Wait for all threads to finish  */ 
    for (int i = 0; i < num_threads; i++) {
        int ret = pthread_join(threads_id[i], NULL);
        assert(ret == 0);
    }

    return 0;
}