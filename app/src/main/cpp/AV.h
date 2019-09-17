//
// Created by Administrator on 2019/9/17.
//

#ifndef TESTPLAYER_AV_H
#define TESTPLAYER_AV_H

class AV {
public:
    virtual void release() = 0;

    virtual void pause() = 0;

    virtual void resume() = 0;

//    virtual void onPause() = 0;

//    virtual void onResume() = 0;
};

#endif //TESTPLAYER_AV_H
