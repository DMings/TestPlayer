//
// Created by Administrator on 2019/9/29.
//

#ifndef TESTGIF_PTHREADSLEEP_H
#define TESTGIF_PTHREADSLEEP_H

#include <pthread.h>

class PthreadSleep {
public:
    PthreadSleep();

    ~PthreadSleep();

    void sleep(unsigned int ms);

    void reset();

    void interrupt();

private:
    pthread_mutex_t sleep_mutex;
    pthread_cond_t sleep_cond;
    bool is_interrupt;
};

#endif //TESTGIF_PTHREADSLEEP_H
