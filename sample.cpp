#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <linux/videodev2.h>
#include <time.h>
#include <sys/mman.h>
#include <string.h>
#include "color.h"
#include "verifier.h"
#include "usermanager.h"
#include "detector.h"
#include <opencv2/opencv.hpp>
#include <vector>
#include "memwatch.h"
using namespace std;
#define CLEAR(x) memset (&(x), 0, sizeof (x))
GThread *thread=0;
char dev_name[128] = "/dev/video0";
int width = 320;
int height = 240;
int preview_width = 320;
int preview_height = 240;
int FPS = 30;//Max Power
int USER=-1;
int MAXRETRYTIME=2000;
int retryTime=0;
struct buffer {
        void *                  start;
        size_t                  length;
};

buffer *buffers = NULL;

int fd = -1;
int reqbuf_count = 4;
int refreshcount=0;

GtkWidget *window;
GtkWidget *image_face;
GMutex *mutex = NULL;
//=======Widget==
GtkWidget *statuslabel;
GtkWidget *statusframe;
GtkWidget *hPaned;
GtkWidget *vbox;
GtkWidget *svbox;
GtkWidget *svframe;
GtkWidget *svlabel;
GtkWidget *hbox;
GtkWidget *polyuimage;
GtkWidget *scrolledWindow;
GtkWidget *textView;
bool isEnable=false;
bool isPunish=false;
bool hasCreate=false;
int PunishTime=0;
//========GThread=========
gpointer camera_thread(gpointer arg);
unsigned char *buf2=NULL;
int statuscode=-1;
//========================
IplImage opencv_image;
vector<verifier *> vlist;
vector<string> vname;
bool AuthFace();
void opencv_init();
static detector newDetector;
static UserManager manager;
static GdkPixmap *pixmap = NULL;
unsigned char framebuffer[2048 * 1536 * 3];

guint timeId = 0;

void open_device();
void init_device();
void set_format();
void request_buffer();
void query_buf_and_mmap();
void queue_buffer();
void stream_on();
void read_frame();
static gboolean show_camera(gpointer data);
void stream_off();
void mem_unmap_and_close_dev();
int xioctl(int fd, int request, void* arg);
//===============================================
static gboolean change_status(gpointer data);
static gboolean show_finish(gpointer data);

//===============================================


int main( int argc, char *argv[])
{
	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	//layout = gtk_layout_new (NULL, NULL);
//==========================
 	hPaned = gtk_hpaned_new();
    mutex = g_mutex_new();

    polyuimage=gtk_image_new_from_file("/opt/logo.gif");
    statuslabel=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(statuslabel),"<span foreground='blue' size='x-large'>Innovative Intelligent Computing Center</span>");
    statusframe = gtk_frame_new(" ");
    vbox = gtk_vbox_new(FALSE, 5);
    hbox = gtk_hbox_new(FALSE, 5);
    //===========================
    svbox= gtk_vbox_new(FALSE, 5);
    svframe = gtk_frame_new(" ");
    svlabel=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(svlabel),
          "<span foreground='blue' size='x-large'>System Running</span>");
    //==========================
    scrolledWindow = gtk_scrolled_window_new(NULL, NULL);
    textView = gtk_text_view_new();

//==========================
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
	gtk_window_maximize(GTK_WINDOW (window));
	gtk_window_set_title (GTK_WINDOW (window), "Authentication");

    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(scrolledWindow),GTK_POLICY_NEVER,GTK_POLICY_ALWAYS);
    gtk_text_view_set_editable(GTK_TEXT_VIEW (textView),false);
    gtk_widget_set_sensitive(textView, FALSE);
//===========================================================

//===========================================================
    gtk_container_set_border_width (GTK_CONTAINER (window), 0);
	image_face = gtk_drawing_area_new ();
	gtk_widget_set_size_request(image_face, preview_width, preview_height);
//===================================
    gtk_container_add(GTK_CONTAINER(scrolledWindow), textView);
    gtk_box_pack_start(GTK_BOX(hbox), polyuimage, FALSE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(hbox), statuslabel, TRUE, TRUE, 5);
    gtk_container_add(GTK_CONTAINER(statusframe), hbox);
    gtk_box_pack_start(GTK_BOX(vbox), statusframe, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hPaned, TRUE, TRUE, 5);
	gtk_paned_pack1(GTK_PANED(hPaned), 
                    image_face, TRUE, TRUE);
    //==========================
    gtk_container_add(GTK_CONTAINER(svframe), svlabel);
    gtk_box_pack_start(GTK_BOX(svbox), svframe, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(svbox), scrolledWindow, TRUE, TRUE, 5);

      gtk_paned_pack2(GTK_PANED(hPaned),
                    svbox, TRUE, TRUE);
    gtk_container_add(GTK_CONTAINER(window), vbox);
//====================================
	gtk_widget_show_all(window);

	opencv_init();

	open_device();

   	init_device();

	stream_on();

	// defined in color.h, and used to pixel format changing 
    initLut();

    if(!g_thread_supported()) {
            g_thread_init(NULL);
        }
    isEnable=true;
    g_thread_create(camera_thread, NULL, FALSE, NULL);

	gtk_main ();

	freeLut();

	stream_off();

	mem_unmap_and_close_dev();

	return 0;
}

gpointer camera_thread(gpointer arg) 
{
        if(g_mutex_trylock(mutex))
        {
            read_frame();
            buf2 = new unsigned char[width*height*3];
            //=========================

            Pyuv422torgb24((unsigned char*)framebuffer, buf2, width, height);


            //=========================
            opencv_image.imageData=(char *)buf2;
            if(AuthFace())
            {
                //isEnable=false;

                timeId=g_timeout_add(1000/FPS, show_finish, NULL);
                //
            }
            else
            {
            }
            g_mutex_unlock(mutex);
            timeId = g_timeout_add(1000/FPS, show_camera, NULL);

        }
}

void opencv_init()
{
	if(!manager.loadSetting()) 
	{
	printf("Usermanager \n");
	exit(-1);
	}
	vname=manager.getUserList();
	int j=0;
	for(j=0;j<vname.size();j++)
        {
		vlist.push_back(new verifier(vname.at(j).c_str()));
	}
	cvInitImageHeader( &opencv_image,cvSize( width,height ),IPL_DEPTH_8U, 3, IPL_ORIGIN_TL, 4 );
}

bool AuthFace()
{
	//=========May Not Need===========
     //if(isSuccess) return true;
     if(isPunish)//In order to adjust!!
     {
         PunishTime++;
         if(PunishTime>15)
             isPunish=false;
         return false;
     }
	//================================
	 newDetector.runDetector(&opencv_image);
         if (sqrt(pow(newDetector.eyesInformation.LE.x-newDetector.eyesInformation.RE.x,2) + (pow(newDetector.eyesInformation.LE.y-newDetector.eyesInformation.RE.y,2)))>28  && sqrt(pow(newDetector.eyesInformation.LE.x-newDetector.eyesInformation.RE.x,2) + (pow(newDetector.eyesInformation.LE.y-newDetector.eyesInformation.RE.y,2)))<120)
         {  
             double yvalue=newDetector.eyesInformation.RE.y-newDetector.eyesInformation.LE.y;
             double xvalue=newDetector.eyesInformation.RE.x-newDetector.eyesInformation.LE.x;
             double ang= atan(yvalue/xvalue)*(180/CV_PI);
             if (pow(ang,2)<200)
             {
                 IplImage * im = newDetector.clipFace(&opencv_image);
                 if (im!=0)
                 {
                       int j=0;
                       printf("Checking Face!\n");
                       statuscode=1;
                       g_timeout_add(1000/FPS, change_status, NULL);
                       for(j=0;j<vlist.size();j++)
                       {

                           int val=vlist.at(j)->verifyFace(im);
                           if (val==1)
                           {
                               cvReleaseImage(&im);
                                USER=j;
                               printf("Auth Successful\n");
                                printf("\n**********************************************************\n");
                                printf("Welcome:%s\n",vname.at(j).c_str());
                                printf("**********************************************************\n\n");
                                isPunish=true;
                                PunishTime=-250;
                                statuscode=2;
                                g_timeout_add(1000/FPS, change_status, NULL);
                               return true;
                           }
                       }
                       cvReleaseImage(&im);
                       printf("Access Denied\n");
                       isPunish=true;
                       PunishTime=-50;
                       statuscode=3;
                        g_timeout_add(1000/FPS, change_status, NULL);
                       return false;
                  }
                  else
                  {
                          printf("No head in image!\n");
                          return false;
                  }
             }
             else
             {
                 printf("Please keep align face!\n");
                 isPunish=true;
                 PunishTime=-50;
                 statuscode=5;
                 g_timeout_add(1000/FPS, change_status, NULL);
                 return false;
             }
         }
         else
         {
             printf("Adjusting distance\n");
             isPunish=true;
             PunishTime=-20;
             statuscode=4;
             g_timeout_add(1000/FPS, change_status, NULL);
             return false;
         }
}

void open_device()
{
	fd = open (dev_name, O_RDWR|O_NONBLOCK, 0);
	if(-1 == fd) {
		printf("open device error\n");
		/* Error handler */ 
	}
}

void init_device()
{
	set_format();
	request_buffer();
	query_buf_and_mmap();
	queue_buffer();
}

void set_format()
{
	struct v4l2_format fmt;
	CLEAR (fmt);

	fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width       = width;
	fmt.fmt.pix.height      = height;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;

 	if(-1 == xioctl (fd, VIDIOC_S_FMT, &fmt))
 	{
		printf("set format error\n");
   		// Error handler
	}
}

void request_buffer()
{
	struct v4l2_requestbuffers req;
	CLEAR (req);

	req.count               = reqbuf_count;
	req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory              = V4L2_MEMORY_MMAP;

	if(-1 == xioctl (fd, VIDIOC_REQBUFS, &req))
	{
		printf("request buf error\n");
		// Error handler
	}
}

void query_buf_and_mmap()
{
	buffers = (buffer*) calloc(reqbuf_count, sizeof(*buffers) );
	if(!buffers)
	{
		// Error handler
	}

	struct v4l2_buffer buf;
	for(int i = 0; i < reqbuf_count; ++i)
	{
		CLEAR (buf);

		buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index  = i;

		if(-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
        {
			printf("query buf error\n");
        	// Error handler
		}
		//printf("buffer length: %d\n", buf.length);
		//printf("buffer offset: %d\n", buf.m.offset);	

		buffers[i].length = buf.length;
		buffers[i].start = mmap(NULL,
									buf.length,
									PROT_READ|PROT_WRITE,
									MAP_SHARED,
									fd,
									buf.m.offset);

		if(MAP_FAILED == buffers[i].start)
		{
			// Error handler
		}
	}
}

void queue_buffer()
{
	struct v4l2_buffer buf;

	for(int i = 0; i < reqbuf_count; ++i) {

		CLEAR (buf);

		buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index  = i;

		if(-1 == xioctl(fd, VIDIOC_QBUF, &buf))
		{
			// Error handler
		}
	}
}

void stream_on()
{
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(-1 == xioctl(fd, VIDIOC_STREAMON, &type))
	{
		// Error handler
	}
}

void read_frame()
{	
	struct v4l2_buffer buf;
	unsigned int i;
       
	CLEAR (buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	if(-1 == xioctl (fd, VIDIOC_DQBUF, &buf)) 
	{
		// Error handler
	}
//	assert (buf.index < n_buffers0);
   
	memcpy(framebuffer, buffers[buf.index].start, buf.bytesused);
	
	if(-1 == xioctl (fd, VIDIOC_QBUF, &buf))
	{
		// Error handler
	} 
}

static gboolean show_camera(gpointer data)
{
    if(g_mutex_trylock(mutex))
    {
        //====Secure=====
        g_source_remove(timeId);
        //=========
        if(pixmap) {
            g_object_unref(pixmap); // ref count minus one
        }

        pixmap = gdk_pixmap_new (image_face->window, preview_width, preview_height, -1);

        GdkPixbuf *rgbBuf = gdk_pixbuf_new_from_data(buf2, GDK_COLORSPACE_RGB, FALSE, 8,width, height, width*3, NULL, NULL);

        if(rgbBuf != NULL)
        {
            /*GdkPixbuf* buf = gdk_pixbuf_scale_simple(rgbBuf,
                                                                preview_width,
                                                                preview_height,
                                                                GDK_INTERP_BILINEAR);*/
            gdk_draw_pixbuf(pixmap,
                                image_face->style->white_gc,
                                rgbBuf,
                                0, 0, 0, 0,
                                preview_width,
                                preview_height,
                                GDK_RGB_DITHER_NONE,
                                0, 0);
            gdk_draw_drawable(image_face->window,
                                  image_face->style->white_gc,
                                  pixmap,
                                  0, 0, 60, 30,
                                  preview_width,
                                  preview_height);
            //g_object_unref(buf);
            g_object_unref(rgbBuf);
        }

        gtk_widget_show(image_face);

        delete [] buf2;
        g_mutex_unlock(mutex);
        if(isEnable)
        {
            g_thread_create(camera_thread, NULL, FALSE, NULL);
            //restart the thread
        }

        return FALSE;
     }
	return FALSE;
}

void stream_off(void)
{
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
	{
		// Error handler
	}
}

void mem_unmap_and_close_dev()
{
	for(int i = 0; i < reqbuf_count; ++i)
	{
		if(-1 == munmap(buffers[i].start, buffers[i].length))
		{
			// Error hanlder
		}
	}

	free(buffers);
	close(fd);
}

int xioctl(int fd, int request, void* arg)
{
	int r;

	do r = ioctl (fd, request, arg);
	while (-1 == r && EINTR == errno);

	return r;
}
static gboolean show_finish(gpointer data)
{
    char greeting[200];
    system("python /opt/OpenDoor.py &");
    return false;
}
static gboolean change_status(gpointer data)
{
    refreshcount++;
    GtkTextBuffer *buffer;
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW (textView));
    //gtk_label_set_text(GTK_LABEL(svlabel),"Welcome: TesterJiaqi");
    if(refreshcount>100)
    {
        char standbytext[200]="~~~Face Authentication Ready Now~~~!\n";
        gtk_text_buffer_set_text (buffer,standbytext,-1);
        refreshcount=0;
    }
    GtkTextMark *mark;
    GtkTextIter iter;
    mark = gtk_text_buffer_get_insert(buffer);
    gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
    if(!hasCreate)
    {
        gtk_text_buffer_create_tag(buffer,"red_background","foreground","red",NULL);
        gtk_text_buffer_create_tag(buffer,"blue_background","foreground","blue",NULL);
        char standbytext[200]="~~~Face Authentication Ready Now~~~!\n";
        gtk_text_buffer_set_text (buffer,standbytext,-1);
        hasCreate=true;
    }
    time_t timep;
    struct tm *p;
    time(&timep);
    p=localtime(&timep);
    switch(statuscode)
    {
        case 1:
        {
            char text[200];
            sprintf(text,"%d:%d:%d:Checking Face.Please Wait!\n",p->tm_hour,p->tm_min,p->tm_sec);
            gtk_text_buffer_insert (buffer,&iter,text,-1);
        }
            break;
        case 2:
        {
        char text[200];
        char text1[200];
        sprintf(text,"%d:%d:%d:Welcome %s.\n",p->tm_hour,p->tm_min,p->tm_sec,vname.at(USER).c_str());
        sprintf(text1,"<span foreground='blue' size='x-large'>Welcome %s.</span>",vname.at(USER).c_str());
        gtk_text_buffer_insert_with_tags_by_name(buffer,&iter,text,
        -1,"blue_background",NULL);
        gtk_label_set_markup(GTK_LABEL(svlabel),
             text1);
        }
            break;
        case 3:
        {
            char text[200];
            sprintf(text,"%d:%d:%d:Access Denied.\n",p->tm_hour,p->tm_min,p->tm_sec);
            //gtk_text_buffer_insert (buffer,&iter,text,-1);
            gtk_text_buffer_insert_with_tags_by_name(buffer,&iter,text,
            -1,"red_background",NULL);
        }
            break;
        case 4:
        {
        char text[200];
        //char text1[200];
        sprintf(text,"%d:%d:%d:Adjust the distance.\n",p->tm_hour,p->tm_min,p->tm_sec);
        gtk_text_buffer_insert (buffer,&iter,text,-1);
        if(newDetector.messageIndex==0)
        {
            gtk_text_buffer_insert (buffer,&iter,"Come closer please\n",-1);
        }
        else if((newDetector.messageIndex==1))
        {
            gtk_text_buffer_insert (buffer,&iter,"Go a litter far please\n",-1);
        }
        else
        {
            gtk_text_buffer_insert (buffer,&iter,"Unable to detect face\n",-1);
        }

        }
            break;
        case 5:
        {
            char text[200];
            sprintf(text,"%d:%d:%d:Keep align face please.\n",p->tm_hour,p->tm_min,p->tm_sec);
            gtk_text_buffer_insert (buffer,&iter,text,-1);
        }
            break;
        default:
        {
        char text[200]="Face Authentication Ready.Please Wait!\n";
        gtk_text_buffer_insert (buffer,&iter,text,-1);
        }

    }
    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(textView), mark);
 
    return FALSE;
}
