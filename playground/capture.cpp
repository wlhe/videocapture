#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <sys/time.h>

#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/videoio.hpp"


#include <fstream>
#include <iostream>
#include <thread>
#include <queue>
#include <mutex>

using namespace std;
using namespace cv;

#define CAMERA  "/dev/video0"
#define CAPTURE_FILE    "frame_t2"

#define MAX_HEIGHT  1024
#define MAX_WIDTH   768

#define BUFFER_COUNT 4
    
typedef struct __video_buffer
{
    void *start;
    size_t length;

}video_buf_t;

struct  Frame
{   
    size_t length;
    int height;
    int width;
    unsigned char data[MAX_HEIGHT * MAX_WIDTH * 3];
};

Frame frame;
std::mutex f_lock;
std::queue<Frame> f_queue;
#define MAX_FRAME_QUEUE_SIZE    64

int width = 640;
int height = 480;

static int fd;

bool v4l2_init();
bool v4l2_start();
void v4l2_cvshow();
void v4l2_save();
double get_current_time();
void opencv_cap();
void yuyv_to_bgr(unsigned char *yuyv,unsigned char *rgb,int height,int width);

video_buf_t *framebuf;
char *camera = (char *)CAMERA;
char *file = (char *)CAPTURE_FILE;

int main(int argc, char *argv[])
{
    if(argc == 2)
        camera = argv[1];
    else if(argc == 3)
    {
        camera = argv[1];
        file = argv[2];
    }
    v4l2_init();
    v4l2_start();
    //v4l2_cvshow();
    thread *t1 = new thread(v4l2_cvshow);
    //thread *t2 = new thread(v4l2_save);
    //t2->join();
    t1->join();

    // thread *t3 = new thread(opencv_cap);
    // t3->join();



    //while(1);
    
    return 0;
}


bool v4l2_init()
{
    if((fd = open(camera,O_RDWR)) == -1)
    {
        perror("Camera open failed!\n");
        return false;
    }

    //query camera capabilities
    struct v4l2_capability cap;

    if(ioctl(fd,VIDIOC_QUERYCAP,&cap) == -1)
    {
        perror("VIDIOC_QUERYCAP failed!\n");
        return false;
    }

    // printf("Capability Informations:\n");
    // printf(" driver: %s\n", cap.driver);
    // printf(" card: %s\n", cap.card);
    // printf(" bus_info: %s\n", cap.bus_info);
    // printf(" version: %08X\n", cap.version);
    // printf(" capabilities: %08X\n", cap.capabilities);

    if(cap.capabilities & V4L2_PIX_FMT_JPEG)
    {
        printf("V4L2_PIX_FMT_JPEG yes\n");
    }
    else 
    {
        printf("V4L2_PIX_FMT_JPEG no\n");
    }

    //set format
    struct v4l2_format fmt;
    memset(&fmt,0,sizeof(fmt));

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat =  V4L2_PIX_FMT_YUYV;//V4L2_PIX_FMT_MJPEG;//;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;//V4L2_FIELD_INTERLACED;

    if(ioctl(fd,VIDIOC_S_FMT,&fmt) == -1)
    {
        perror("VIDIOC_S_FMT failed!\n");
        return false;
    }

    //get format
    if(ioctl(fd,VIDIOC_G_FMT,&fmt) == -1)
    {
        perror("VIDIOC_G_FMT failed!\n");
        return false;
    }
    height = fmt.fmt.pix.height;
    width = fmt.fmt.pix.width;

    printf("Stream Format Informations:\n");
    printf(" type: %d\n", fmt.type);
    printf(" width: %d\n", fmt.fmt.pix.width);
    printf(" height: %d\n", fmt.fmt.pix.height);
    char fmtstr[8];
    memset(fmtstr, 0, 8);
    memcpy(fmtstr, &fmt.fmt.pix.pixelformat, 4);
    printf(" pixelformat: %s\n", fmtstr);
    printf(" field: %d\n", fmt.fmt.pix.field);
    printf(" bytesperline: %d\n", fmt.fmt.pix.bytesperline);
    printf(" sizeimage: %d\n", fmt.fmt.pix.sizeimage);
    printf(" colorspace: %d\n", fmt.fmt.pix.colorspace);
    printf(" priv: %d\n", fmt.fmt.pix.priv);
    printf(" raw_date: %s\n", fmt.fmt.raw_data);

    // struct v4l2_crop 

    return true;
}

bool v4l2_start()
{
    //request memory allocation
    struct v4l2_requestbuffers reqbuf;
    reqbuf.count = BUFFER_COUNT;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;

    if(ioctl(fd,VIDIOC_REQBUFS,&reqbuf) == -1)
    {
        perror("VIDIOC_REQBUFS failed!\n");
        return false;
    }

    framebuf = (video_buf_t *)calloc(reqbuf.count,sizeof(video_buf_t));
    struct v4l2_buffer buf;

    for(int i = 0;i < reqbuf.count;i ++)
    {
        buf.index = i;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if(ioctl(fd,VIDIOC_QUERYBUF,&buf) == -1)
        {
            perror("VIDIOC_QUERYBUF failed!\n");
            return false;
        }

        //mmap buffer
        framebuf[i].length = buf.length;
        framebuf[i].start = mmap(NULL,buf.length,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,fd,buf.m.offset);
        if(framebuf[i].start == MAP_FAILED)
        {
            perror("mmap failed!\n");
            return false;
        }
        //buffer queue
        if(ioctl(fd,VIDIOC_QBUF,&buf) == -1)
        {
            perror("VIDIOC_QBUF failed!\n");
            return false;
        }
    }
    //start camera capture
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(fd,VIDIOC_STREAMON,&type) == -1)
    {
        perror("VIDIOC_STREAMON failed!\n");
        return false;
    }

    return true;
}

void v4l2_cvshow()
{
    bool quit = false;
    struct v4l2_buffer buf;
    CvMat cvmat;
    IplImage* image;
    int retry = 0;
    int buffercounter = 0;
    double start = 0, end = 0, dt = 0;

    while(!quit)
    {
        start = get_current_time();
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if(ioctl(fd,VIDIOC_DQBUF,&buf) == -1)
        {
            perror("VIDIOC_DQBUF failed!\n");
            usleep(10000);
            retry ++;
            if(retry > 10)
                quit = true;
            continue;
        }
        
        frame.length = buf.length;
        memcpy(frame.data,framebuf[buf.index].start,buf.length);

        f_lock.lock();
        if(f_queue.size() >= MAX_FRAME_QUEUE_SIZE)
            f_queue.pop();
        f_queue.push(frame);
        f_lock.unlock();

        //printf("buf.length = %d, height * width * 3 = %d\n",buf.length,height * width * 3);

        unsigned char * bgr_image = new unsigned char[height * width * 3];
        unsigned char * yuyv_image = (unsigned char *)framebuf[buf.index].start;
        yuyv_to_bgr(yuyv_image,bgr_image,height,width);

        Mat img(height,width,CV_8UC3,bgr_image);
        imshow("Image",img);
        
        // cvmat = cvMat(height,width,CV_8UC3,framebuf[buf.index].start);
        // image = cvDecodeImage(&cvmat,1);
        // cvShowImage("Image",image);
        // cvReleaseImage(&image);

        if(ioctl(fd,VIDIOC_QBUF,&buf) == -1)
        {
            perror("VIDIOC_QBUF failed!\n");
            continue;
        }
        end = get_current_time();

        if(end > start)
        {
            dt = 0.95 * dt + 0.05 * (end - start);
        }
        int fps = int(1000.0/dt);
        printf("buffer %d captured,elapsed time = %lf ms, FPS = %d\n",++buffercounter,dt,fps );
        waitKey(10);
    }
    close(fd);

}

void v4l2_save()
{
    bool saving = true;
    Frame frm;
    int qsize = 0;
    ofstream *ofs = new ofstream();
    int framecounter = 0;
    double start = 0, end = 0, dt = 0;

    while(saving)
    {
        start = get_current_time();
        f_lock.lock();
        if(f_queue.empty())
        {
            f_lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        else
        {
            frm = f_queue.front();
            f_queue.pop();
        }
        f_lock.unlock();

         stringstream name;
         name << framecounter << ".jpeg";

         ofs->open(name.str(),ofstream::out | ofstream::app);
        /*if(framecounter == 0)
        {
            ofs->open(file,ofstream::out);
        }
        else
        {
            ofs->open(file,ofstream::out | ofstream::app);
        }
       */ 
        if(ofs->is_open())
        {
            ofs->write((const char *)frm.data,frm.length);
        }
        ofs->close();
        end = get_current_time();
        dt = end - start;

        printf("frame %d saved,elapsed time = %lf ms\n",++framecounter,dt);
        //saving = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

double get_current_time()
{
    double t = 0.0;
    struct timeval tv;
    gettimeofday(&tv,NULL);
    t = tv.tv_sec * 1000 + tv.tv_usec/1000.0;
    return t;
}

void opencv_cap()
{
    cout << "Built with OpenCV " << CV_VERSION << endl;
    Mat image;
    VideoCapture capture;
    double start = 0, end = 0, dt = 0;

    capture.open(2);
    if(capture.isOpened())
    {
        cout << "Capture is opened" << endl;
        for(;;)
        {
            start = get_current_time();
            capture >> image;
            if(image.empty())
                break;
            end = get_current_time();
            dt = 0.95 * dt + 0.05 * (end - start);
            int fps = int(1000.0/dt);
            cout << "time = "<< dt << "\t" << "fps = " << fps << endl; 
            imshow("Sample", image);
            if(waitKey(10) >= 0)
                break;
        }
    }
}

void yuyv_to_bgr(unsigned char *yuyv,unsigned char *rgb,int height,int width)
{
	int y,v,u;
	float r,g,b;
	size_t yuv_size = height * width * 2;
	size_t rgb_size = height * width * 3;
	for(int i = 0, j = 0;i < rgb_size && j < yuv_size; i += 6,j += 4)
	{
		/*
			y0,u0,y1,v0 		, y2,u1,y3,u1
			r0,g0,b0, r1,g1,b1,
		*/
		//first pixel
		y = yuyv[j];	//y0
		u = yuyv[j + 1];//u0
		v = yuyv[j + 3];//v0

		r = y + 1.4065 * (v - 128);	//r0
		g = y - 0.3455 * (u - 128) - 0.7169 * (v - 128); //g0
		b = y + 1.1790 * (u - 128);	//b0

		if(r < 0) r = 0;
		else if(r > 255) r = 255;
		if(g < 0) g = 0;
		else if(g > 255) g = 255;
		if(b < 0) b = 0;
		else if(b > 255) b = 255;

		rgb[i + 0] = (unsigned char)b;
		rgb[i + 1] = (unsigned char)g;
		rgb[i + 2] = (unsigned char)r;

		//second pixel
		u = yuyv[j + 1];	//u0
		y = yuyv[j + 2];	//y1
		v = yuyv[j + 3];	//v0

		r = y + 1.4065 * (v - 128);	//r1
		g = y - 0.3455 * (u - 128) - 0.7169 * (v - 128); //g1
		b = y + 1.1790 * (u - 128);	//b1

		if(r < 0) r = 0;
		else if(r > 255) r = 255;
		if(g < 0) g = 0;
		else if(g > 255) g = 255;
		if(b < 0) b = 0;
		else if(b > 255) b = 255;

		rgb[i + 3] = (unsigned char)b;
		rgb[i + 4] = (unsigned char)g;
		rgb[i + 5] = (unsigned char)r;
	}
}