/*
    File: client.cpp

    Author: R. Bettati
            Department of Computer Science
            Texas A&M University
    Date  : 2013/01/31

 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

    /* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*
    This assignment does not require any additional includes,
    though you are free to add any that you find helpful.
*/
/*--------------------------------------------------------------------------*/

#include <cassert>
#include <cstring>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include "reqchannel.h"
#include <pthread.h>
#include <string>
#include <sstream>
#include <sys/time.h>
#include <assert.h>
#include <cmath>
#include <numeric>
#include <list>
#include <vector>
#include <queue>


/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

//Request Buffer Class
class request_buffer_class {
    std::queue<std::string> request_buffer;
    pthread_mutex_t buffer_lock;
public:
    request_buffer_class() { pthread_mutex_init(&buffer_lock, NULL); }
    void push_back(std::string s) {
        pthread_mutex_lock(&buffer_lock);
        request_buffer.push(s);
        pthread_mutex_unlock(&buffer_lock);
    }
    void pop_front() {
        pthread_mutex_lock(&buffer_lock);
        request_buffer.pop();
        pthread_mutex_unlock(&buffer_lock);
    }
    std::string front() {
        pthread_mutex_lock(&buffer_lock);
        std::string temp = request_buffer.front();
        pthread_mutex_unlock(&buffer_lock);
        return temp;
    }
 
};

struct PARAMS {
    /*
        This is a helpful struct to have,
        but you can implement the program
        some other way it you'd like.
     
        For example, it can hold pointers to
        the containers that hold the histogram information
        so that all the threads combine their data
        instead of assembling "w" different histograms,
        as well as pointers to RequestChannels and
        to synchronization objects.
     */
    //PARAMS Constructor
    PARAMS(int* n, int* w,std::vector<int>* john_frequency_count, std::vector<int>* jane_frequency_count, std::vector<int>* joe_frequency_count,request_buffer_class* request_buffer){
        this->n = n;
        this->w = w;
        this->john_frequency_count = john_frequency_count;
        this->jane_frequency_count = jane_frequency_count;
        this->joe_frequency_count = joe_frequency_count;
        this->request_buffer = request_buffer;
    }
    //Variables
    int* n;
    int* w;
    request_buffer_class* request_buffer;
    std::vector<int>* john_frequency_count;
    std::vector<int>* jane_frequency_count;
    std::vector<int>* joe_frequency_count;
    RequestChannel* channel;
    pthread_mutex_t request_buffer_lock;
    pthread_mutex_t John_lock;
    pthread_mutex_t Jane_lock;
    pthread_mutex_t Joe_lock;
    std::string people;
};

/*
    The class below allows any thread in this program to use
    the variable "atomic_standard_output" down below to write output to the
    console without it getting jumbled together with the output
    from other threads.
    For example:
    thread A {
        atomic_standard_output.print("Hello ");
    }
    thread B {
        atomic_standard_output.print("World!\n");
    }
    The output could be:
    Hello World!
    or...
    World!
    Hello 
    ...but it will NOT be something like:
    HelWorllo d!
 */

class atomic_standard_output {
    /*
         Note that this class provides an example
         of the usage of mutexes, which are a crucial
         synchronization type. You will probably not
         be able to write the worker thread function
         without using these.
     */
    pthread_mutex_t console_lock;
public:
    atomic_standard_output() { pthread_mutex_init(&console_lock, NULL); }
    void print(std::string s){
        pthread_mutex_lock(&console_lock);
        std::cout << s << std::endl;
        pthread_mutex_unlock(&console_lock);
    }
};

atomic_standard_output threadsafe_standard_output;

/*--------------------------------------------------------------------------*/
/* HELPER FUNCTIONS */
/*--------------------------------------------------------------------------*/

std::string make_histogram(std::string name, std::vector<int> *data) {
    std::string results = "Frequency count for " + name + ":\n";
    for(int i = 0; i < data->size(); ++i) {
        results += std::to_string(i * 10) + "-" + std::to_string((i * 10) + 9) + ": " + std::to_string(data->at(i)) + "\n";
    }
    return results;
}

void* worker_thread_function(void* arg) {
    /*
        Fill in the loop. Make sure
        it terminates when, and not before,
        all the requests have been processed.
        
        Each thread must have its own dedicated
        RequestChannel. Make sure that if you
        construct a RequestChannel (or any object)
        using "new" that you "delete" it properly,
        and that you send a "quit" request for every
        RequestChannel you construct regardless of
        whether you used "new" for it.
     */

    PARAMS* p = (PARAMS*)arg;
    pthread_mutex_lock(&p->request_buffer_lock);
    std::string s = p->channel->send_request("newthread");
    RequestChannel *workerChannel = new RequestChannel(s, RequestChannel::CLIENT_SIDE);
    pthread_mutex_unlock(&p->request_buffer_lock);
    while(true) {
        pthread_mutex_lock(&p->request_buffer_lock);
        std::string request = p->request_buffer->front();
        p->request_buffer->pop_front();
        pthread_mutex_unlock(&p->request_buffer_lock);
        std::string response = workerChannel->send_request(request);
        
        if(request == "data John Smith") {
            pthread_mutex_lock(&p->John_lock);
            p->john_frequency_count->at(stoi(response) / 10) += 1;
            pthread_mutex_unlock(&p->John_lock);
        }
        else if(request == "data Jane Smith") {
            pthread_mutex_lock(&p->Jane_lock);
            p->jane_frequency_count->at(stoi(response) / 10) += 1;
            pthread_mutex_unlock(&p->Jane_lock);
        }
        else if(request == "data Joe Smith") {
            pthread_mutex_lock(&p->Joe_lock);
            p->joe_frequency_count->at(stoi(response) / 10) += 1;
            pthread_mutex_unlock(&p->Joe_lock);
        }
        else if(request == "quit") {
            delete workerChannel;
            break;
        }
    }
}


void* request_thread_function(void *arg) {    
    //std::cout << "Populating request buffer... "; //Commented out for time efficiency
    PARAMS* p = (PARAMS*)arg;
    fflush(NULL);
    for(int i = 0; i < *(p->n); ++i) {
        p->request_buffer->push_back(p->people);
    }
} 

/*--------------------------------------------------------------------------*/
/* MAIN FUNCTION */
/*--------------------------------------------------------------------------*/

int main(int argc, char * argv[]) {
    /*
        Note that the assignment calls for n = 10000.
        That is far too large for a single thread to accomplish quickly,
        but you will still have to test it for w = 1 once you finish
        filling in the worker thread function.
     */
    //Initialize variables
    int* n = new int(100);
    int* w = new int(1);
    request_buffer_class* request_buffer = new request_buffer_class();
    std::vector<int>* john_frequency_count = new std::vector<int> (10, 0);
    std::vector<int>* jane_frequency_count = new std::vector<int> (10, 0);
    std::vector<int>* joe_frequency_count = new std::vector<int> (10, 0);
    //Initialize the PARAMS
    PARAMS* Params1 = new PARAMS(n,w,john_frequency_count, jane_frequency_count, joe_frequency_count, request_buffer);
    PARAMS* Params2 = new PARAMS(n,w,john_frequency_count, jane_frequency_count, joe_frequency_count, request_buffer);
    PARAMS* Params3 = new PARAMS(n,w,john_frequency_count, jane_frequency_count, joe_frequency_count, request_buffer);
    
    //Parse the input
    int opt = 0;
    while ((opt = getopt(argc, argv, "n:w:h")) != -1) {
        switch (opt) {
            case 'n':
                *n = atoi(optarg);
                break;
            case 'w':
                *w = atoi(optarg);
                break;
            case 'h':
            default:
                std::cout << "This program can be invoked with the following flags:" << std::endl;
                std::cout << "-n [int]: number of requests per patient" << std::endl;
                std::cout << "-w [int]: number of worker threads" << std::endl;
                std::cout << "-h: print this message and quit" << std::endl;
                std::cout << "Example: ./client_solution -n 10000 -w 120 -v 1" << std::endl;
                std::cout << "If a given flag is not used, or given an invalid value," << std::endl;
                std::cout << "a default value will be given to the corresponding variable." << std::endl;
                std::cout << "If an illegal option is detected, behavior is the same as using the -h flag." << std::endl;
                exit(0);
        }
    }
    
    int pid = fork();
    if(pid == 0){
        
        struct timeval start_time;
        struct timeval finish_time;
        int64_t start_usecs;
        int64_t finish_usecs;
        
        std::cout << "n == " << *n << std::endl;
        std::cout << "w == " << *w << std::endl;
        
        std::cout << "CLIENT STARTED:" << std::endl;
        std::cout << "Establishing control channel... " << std::flush;
        RequestChannel *chan = new RequestChannel("control", RequestChannel::CLIENT_SIDE);
        Params1->channel = chan;
        std::cout << "done." << std::endl;
        
        /*
            All worker threads will use these,
            but the current implementation puts
            them on the stack in main as opposed
            to global scope. They can be moved to
            global scope, but you could also pass
            pointers to them to your thread functions.
         */
        
        
        /*-------------------------------------------*/
        /* START TIMER HERE */
        /*-------------------------------------------*/

        gettimeofday (&start_time, NULL);

        
/*--------------------------------------------------------------------------*/
/* BEGIN "MODIFY AT YOUR OWN RISK" SECTION */
/*
    But please remember to comment out the output statements
    while gathering data for your report. They throw off the timer.
*/
/*--------------------------------------------------------------------------*/

    //Create the different threads with the different person name
    pthread_t thread[3]; 
    Params1->people = "data John Smith";
    pthread_create( &(thread[0]), NULL, request_thread_function, (void *)Params1);
    Params2->people = "data Jane Smith";
    pthread_create( &(thread[1]), NULL, request_thread_function, (void *)Params2);
    Params3->people = "data Joe Smith";
    pthread_create( &(thread[2]), NULL, request_thread_function, (void *)Params3);

    sleep(5);
    
    //Join the threads
    pthread_join(thread[0], NULL);
    pthread_join(thread[1], NULL);
    pthread_join(thread[2], NULL);

    //Push Quits into the request buffer
    //std::cout << "Pushing quit requests... "; //Commented out for time efficiency
    fflush(NULL);
    for(int i = 0; i < *w; ++i) {
       request_buffer->push_back("quit");
    }
    //std::cout << "done." << std::endl; //Commented out for time efficiency

/*--------------------------------------------------------------------------*/
/* END "MODIFY AT YOUR OWN RISK" SECTION    */
/*--------------------------------------------------------------------------*/
        
/*--------------------------------------------------------------------------*/
/*  BEGIN CRITICAL SECTION  */
/*
    Most of the changes you make will be
    to this section of code and to the
    worker thread function from earlier, unless
    you also decide to utilize the PARAMS struct (which is
    *highly* recommended) and then you'll need
    to fill that in as well.
 
    You will modify the program so that client.cpp
    uses w threads to process the requests, instead of
    sequentially using a single loop.
*/
/*--------------------------------------------------------------------------*/

pthread_t worker_thread[*w]; //Initialize the worker thread array
//Initialize the mutex's
pthread_mutex_init(&Params1->request_buffer_lock, NULL);
pthread_mutex_init(&Params1->John_lock, NULL);
pthread_mutex_init(&Params1->Jane_lock, NULL);
pthread_mutex_init(&Params1->Joe_lock, NULL);

//Create the new worker threadss
for (int i = 0; i < *w; ++i)
{
    pthread_create( &(worker_thread[i]), NULL, worker_thread_function, (void *)Params1);
}

sleep(5);
//Join the worker threads
for (int i = 0; i < *w; ++i)
{
    pthread_join(worker_thread[i], NULL);
}
//Destroy the mutex's
pthread_mutex_destroy(&Params1->request_buffer_lock);
pthread_mutex_destroy(&Params1->John_lock);
pthread_mutex_destroy(&Params1->Jane_lock);
pthread_mutex_destroy(&Params1->Joe_lock);

/*--------------------------------------------------------------------------*/
/*  END CRITICAL SECTION    */
/*--------------------------------------------------------------------------*/
        
        /*
            By the point at which you end the timer,
            all worker threads should have terminated.
            Note that the containers from earlier
            are still in scope, so threads can use them
            if they have a pointer to them.
         */
        
        /*-------------------------------------------*/
        /* END TIMER HERE   */
        /*-------------------------------------------*/
                gettimeofday (&finish_time, NULL);
        
        /*
            You may want to eventually add file output
            to this section of the code to make it easier
            to assemble the timing data from different iterations
            of the program.
         */
        
        start_usecs = (start_time.tv_sec * 1e6) + start_time.tv_usec;
        finish_usecs = (finish_time.tv_sec * 1e6) + finish_time.tv_usec;
        std::cout << "Finished!" << std::endl;
        
        std::string john_results = make_histogram("John Smith", john_frequency_count);
        std::string jane_results = make_histogram("Jane Smith Smith", jane_frequency_count);
        std::string joe_results = make_histogram("Joe Smith", joe_frequency_count);
        
        std::cout << "Results for n == " << *n << ", w == " << *w << std::endl;
        std::cout << "Time to completion: " << std::to_string(finish_usecs - start_usecs) << " usecs" << std::endl;
        std::cout << "John Smith total: " << accumulate(john_frequency_count->begin(), john_frequency_count->end(), 0) << std::endl;
        std::cout << john_results << std::endl;
        std::cout << "Jane Smith total: " << accumulate(jane_frequency_count->begin(), jane_frequency_count->end(), 0) << std::endl;
        std::cout << jane_results << std::endl;
        std::cout << "Joe Smith total: " << accumulate(joe_frequency_count->begin(),joe_frequency_count->end(), 0) << std::endl;
        std::cout << joe_results << std::endl;
        
        std::cout << "Sleeping..." << std::endl;
        usleep(10000);
        
        /*
            EVERY RequestChannel must send a "quit"
            request before program termination, and
            the destructor for RequestChannel must be
            called somehow.
         */
        std::string finale = chan->send_request("quit");
        //Delete all of the initalized data
        delete chan;
        delete n;
        delete w;
        delete john_frequency_count;
        delete jane_frequency_count;
        delete joe_frequency_count;
        delete request_buffer;
        delete Params1;
        delete Params2;
        delete Params3;
        std::cout << "Finale: " << finale << std::endl; //However, this line is optional.
    }
    else if(pid != 0) execl("dataserver", NULL);
}
