## TestPlayer

实现效果，代码仅供参考学习，要想稳定建议使用ijkplayer

// pic

以下为实现的大概流程，主要分三条线程

主解码线程
avformat_open_input  //打开文件输入
avformat_find_stream_info // 获取音频/视频流信息
初始化音频/视频信息并创建相应的线程，进入循环解码
av_seek_frame // 先判断是否需要seek
avformat_flush //seek后需要清除缓存
av_read_frame // 读取一个AVPacket
获取AVPacket类型，av_packet_clone复制一份并放进队列进行分发，分发给
视频解码线程或者音频解码线程进行消耗


audio
av_find_best_stream // 获取音频类型下最好的流
avcodec_find_decoder // 获取解码器
avcodec_alloc_context3 // 分配流的context内存
avcodec_parameters_to_context // 设置流context参数
avcodec_open2 // 打开
创建opensl 同步方式创建，输入数据方式为设置回调
RegisterCallback(bqPlayerBufferQueue, slBufferCallback, this);
并设置声道，采样位格式如无符号16位，声音采集频率如44100
av_samples_alloc // 根据音频参数分配转换buffer，这里由于降低音频的延迟，
这里采用双缓冲音频buffer,意味着一个数组数据在播放的同时在准备另外一个数组数据，
两数组交替切换，互不干扰
swr_alloc_set_opts // 设置转换参数
swr_init // 初始化音频转换器
创建解码audio线程
AttachCurrentThread // 连接JNIEnv环境，用于native线程中更新java时间
启动循环，从音频AVPacket获取一个包
avcodec_send_packet //从包中解码，一个包可能包含多帧
avcodec_receive_frame // 解码包，得到帧 AVFrame，该帧包含音频数据，通常是PCM
如果是seek过的，先校准时间，防止后面的延时计算错误
判断是否需要seek，是就跳过下面
更新java层时间
swr_convert // 转换音频buffer,由于AVFrame中的数据不一定是opensl中设置的格式，这里需要转换一下
等待opensl播放结束回调，回调到达设置音频数据并进行播放
此处音频没有同步播放的主PTS,这里将音频的PTS作为主时间。另外swr_convert 转换不了音频采样率，
实现不了音频通过变采样率来达到同步主时间的效果，我认为引入SoundTouch可以解决，这里就没写了
循环结束，此时为音频播放结束或者遇到错误结束，释放资源,停止opensl 播放回调

video
创建OpenGL环境，设置显示大小，跟随 surface的回调而改变(视频切换的时候并不需要改变)
void surface_created(Surface surface);
void surface_changed(Surface surface, int width, int height);
void surface_destroyed();
// 视频初始化
av_find_best_stream // 获取视频类型下最好的流
avcodec_find_decoder // 获取解码器
avcodec_alloc_context3 // 分配流的context内存
avcodec_parameters_to_context // 设置流context参数
avcodec_open2 // 打开
sws_getContext // 获取视频转换swscontext
av_image_alloc // 根据视频参数分配转换buffer，该buffer用于传递给GL纹理
创建解码video线程
AttachCurrentThread // 连接JNIEnv环境，用于native线程中更新java时间
设置GL纹理尺寸，数据buffer
启动循环，从视频AVPacket队列获取一个包
avcodec_send_packet //从包中解码，一个包可能包含多帧
avcodec_receive_frame // 解码包，得到帧 AVFrame，该帧包含视频数据，通常是YUV
如果是seek过的，先校准时间，防止后面的延时计算错误
判断是否需要seek,如果是seek，后面就不必执行了，跳过这帧显示
sws_scale // 把AVFrame中的数据转换为RGBA buffer
如果有音频的情况下，应该由音频线程更新java时间（用音频时间显示），
如果没有音频流，就让视频更新java层时间（用视频时间显示）
```
uint Video::synchronize_video(double pkt_duration) { // us
    uint wanted_delay = 0;
    double diff_ms;
    double duration;
    bool is_seeking;
    AVRational rational = av_stream->r_frame_rate;
    int video_base = (int) 1000.0 * rational.den / rational.num;
    if (video_base) {
        duration = video_base;
    } else {
        duration = pkt_duration;
    }
    duration = duration < 200 ? duration : 200;
    pthread_mutex_lock(&seek_mutex);
    is_seeking = audio_seeking;
    pthread_mutex_unlock(&seek_mutex);

    if (!is_seeking) {
        if (has_audio) {
            diff_ms = get_video_pts_clock() - get_audio_clock();
        } else {
            diff_ms = get_video_pts_clock() - get_master_clock();
        }
    } else { // 在seek的时候，音频是不可靠的，音频也在变，这时候尽量丢弃，反正都是没用的
        diff_ms = duration * 0.3;
    }

    if (!has_audio && fabs(diff_ms) >= duration * 2) {  // 当只有视频的时候，已经明显失去了同步，校准一下
        double time = av_gettime_relative() / 1000.0;
        set_master_clock(time - get_video_pts_clock());
        return (uint) (duration * 0.7);
    }

    if (diff_ms < 0) { // 视频落后时间
        if (fabs(diff_ms) < duration * 0.1) { // 如果差距比较少就不管了
            wanted_delay = 0;
        } else {
            wanted_delay = (uint) (duration * 0.1); // 减少时间，尽量追上
        }
    } else { // 视频超过时间
        if (fabs(diff_ms) < duration * 0.15) { // 如果差距比较少就不管了
            wanted_delay = 0;
        } else { // 增加时间，尽量等待时间追上
            if (diff_ms < duration * 0.8) {
                wanted_delay = (uint) (diff_ms * 0.8);
            } else {
                wanted_delay = (uint) (duration * 0.8); // 最大不超过，不然卡顿非常明显
            }
        }
    }
    return wanted_delay;
}
```
视频同步音频（如果音频存在），音频不存在就直接计算视频延时，进行sleep操作
RGBA buffer转gl纹理并绘制显示
循环结束，此时为视频播放结束或者遇到错误结束，释放资源，绘制黑色背景
