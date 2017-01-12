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

#include <iostream>
using namespace std;
using namespace cv;

#define CAMERA	"/dev/video0"
#define CAPTURE_FILE	"frame.yuv"

#define BUFFER_COUNT 4

typedef struct __video_buffer
{
	void *start;
	size_t length;

}video_buf_t;

int width = 720;
int height = 480;

static int fd;

bool v4l2_init();
bool v4l2_start();
void v4l2_cvshow();

video_buf_t *framebuf;

int main(int argc, char *argv[])
{
	v4l2_init();
	v4l2_start();
	v4l2_cvshow();
	//while(1);
	close(fd);
	return 0;
}


bool v4l2_init()
{
	if((fd = open(CAMERA,O_RDWR)) == -1)
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

    printf("Capability Informations:\n");
    printf(" driver: %s\n", cap.driver);
    printf(" card: %s\n", cap.card);
    printf(" bus_info: %s\n", cap.bus_info);
    printf(" version: %08X\n", cap.version);
    printf(" capabilities: %08X\n", cap.capabilities);
 

    //set format
    struct v4l2_format fmt;
    memset(&fmt,0,sizeof(fmt));

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;//V4L2_PIX_FMT_YUYV;
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

	while(!quit)
	{
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

		// Mat img(height,width,CV_8UC3,framebuf[buf.index].start);
		// imshow("Image",img);
		
		cvmat = cvMat(height,width,CV_8UC3,framebuf[buf.index].start);
		image = cvDecodeImage(&cvmat,1);
		cvShowImage("Image",image);
		cvReleaseImage(&image);

		if(ioctl(fd,VIDIOC_QBUF,&buf) == -1)
		{
			perror("VIDIOC_QBUF failed!\n");
			continue;
		}
		waitKey(10);
	}

}