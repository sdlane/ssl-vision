//========================================================================
//  This software is free: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License Version 3,
//  as published by the Free Software Foundation.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  Version 3 in the file COPYING that came with this distribution.
//  If not, see <http://www.gnu.org/licenses/>.
//========================================================================
// Some portions of software interfacing with XIO originally writtten
// by James Bruce as part of the CMVision library.
//      http://www.cs.cmu.edu/~jbruce/cmvision/
// (C) 2004-2006 James R. Bruce, Carnegie Mellon University
// Licenced under the GNU General Public License (GPL) version 2,
//   or alternately by a specific written agreement.
//========================================================================
// (C) 2016 Haresh Chudgar, UMass Amherst
// Licenced under the GNU General Public License (GPL) version 3,
//   or alternately by a specific written agreement.
//========================================================================
/*!
 \file    capture_gige.cpp
 \brief   C++ Interface: CaptureGigE
 \author  Haresh Chudgar, (C) 2016
 */
//========================================================================

#include "capture_gige.h"
#include "conversions.h"
#include <iostream>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>      //open
#include <unistd.h>     //close

/*
static void
new_buffer_cb (ArvStream *stream, void* buffer, QMutex mutex)
{
  
  ArvBuffer *buffer;
  static int bufferCount = 0;
  buffer = arv_stream_try_pop_buffer (stream);
  if (buffer != NULL) {
	  if (arv_buffer_get_status (buffer) == ARV_BUFFER_STATUS_SUCCESS)
		  bufferCount++;
	  mutex.lock();
	  int arv_row_stride;
	  int width, height;
	  char *buffer_data;
	  size_t buffer_size;

	  buffer_data = (char *) arv_buffer_get_data (buffer, &buffer_size);
	  
	  mutex.unlock();
	  arv_stream_push_buffer (stream, buffer);
  }
}
*/
	  
static void
control_lost_cb (ArvGvDevice *gv_device)
{
	/* Control of the device is lost. Display a message and force application exit */
	printf ("Control lost\n");
}

void CaptureGigE::discoverDevices(char** deviceList, int* nDevices)
{
  arv_update_device_list();
  *nDevices = arv_get_n_devices ();
  deviceList = new char*[*nDevices];
  for(int dev = 0; dev < *nDevices; dev++) {
    //deviceList[dev] = arv_get_device_id(dev);
    cout << "Device " << dev << ": " << arv_get_device_id(dev) << endl;
  }
}

//======================= GUI & API Definitions =======================

#ifndef VDATA_NO_QT
CaptureGigE::CaptureGigE(VarList * _settings,int default_camera_id, QObject * parent) : QObject(parent), CaptureInterface(_settings)
#else
CaptureGigE::CaptureGigE(VarList * _settings,int default_camera_id) : CaptureInterface(_settings)
#endif
{
#ifndef VDATA_NO_QT
    mutex.lock();
#endif
  settings->addChild(conversion_settings = new VarList("Conversion Settings"));
  settings->addChild(capture_settings = new VarList("Capture Settings"));
    
  //=======================CONVERSION SETTINGS=======================
  conversion_settings->addChild(v_colorout=new VarStringEnum("convert to mode",Colors::colorFormatToString(COLOR_RGB8)));
  v_colorout->addItem(Colors::colorFormatToString(COLOR_RGB8));
  
  //=======================CAPTURE SETTINGS==========================
  //TODO: set default camera id
  capture_settings->addChild(v_cam_bus          = new VarInt("cam idx",1));
  capture_settings->addChild(v_fps              = new VarInt("framerate",30));
  capture_settings->addChild(v_width            = new VarInt("width",640));
  capture_settings->addChild(v_height           = new VarInt("height",480));
  capture_settings->addChild(v_left            = new VarInt("left",0));
  capture_settings->addChild(v_top           = new VarInt("top",0));
  capture_settings->addChild(v_colormode        = new VarStringEnum("capture mode",Colors::colorFormatToString(COLOR_RGB8)));
  v_colormode->addItem(Colors::colorFormatToString(COLOR_RGB8));

  //capture_settings->addChild(v_buffer_size      = new VarInt("ringbuffer size",V4L_STREAMBUFS));
  //v_buffer_size->addFlags(VARTYPE_FLAG_READONLY);

#ifndef VDATA_NO_QT
    mutex.unlock();
#endif
}

#ifndef VDATA_NO_QT
void CaptureGigE::mvc_connect(VarList * group) {
    vector<VarType *> v=group->getChildren();
    for (unsigned int i=0;i<v.size();i++) {
        connect(v[i],SIGNAL(wasEdited(VarType *)),group,SLOT(mvcEditCompleted()));
    }
    connect(group,SIGNAL(wasEdited(VarType *)),this,SLOT(changed(VarType *)));
}

void CaptureGigE::changed(VarType * group) {
    if (group->getType()==VARTYPE_ID_LIST) {
        writeParameterValues( (VarList *)group );
        readParameterValues( (VarList *)group );
    }
}
#endif


void CaptureGigE::readAllParameterValues() {
  
}

void CaptureGigE::writeAllParameterValues() {
  
}

void CaptureGigE::readParameterValues(VarList * item) {
  
}

void CaptureGigE::writeParameterValues(VarList * item) {
  
}

CaptureGigE::~CaptureGigE()
{
  rawFrame.clear();           //release memory from local image
}

bool CaptureGigE::resetBus() {
#ifndef VDATA_NO_QT
    mutex.lock();
#endif

#ifndef VDATA_NO_QT
    mutex.unlock();
#endif
    return true;
    
}

bool CaptureGigE::stopCapture()
{
  if (isCapturing() == false) {
    return true;
  }
  
  is_capturing = false;
  
  /*
  readAllParameterValues();

  vector<VarType *> tmp = capture_settings->getChildren();
  for (unsigned int i=0; i < tmp.size();i++) {
      tmp[i]->removeFlags( VARTYPE_FLAG_READONLY );
  }
  dcam_parameters->addFlags( VARTYPE_FLAG_HIDE_CHILDREN );
  */
  arv_camera_stop_acquisition (camera);
  g_object_unref (stream);
  g_object_unref (camera);
  camera = NULL;
  stream = NULL;

  return true;
}

bool CaptureGigE::startCapture()
{
  if (!v_cam_bus) return false;

#ifndef VDATA_NO_QT
  mutex.lock();
#endif
  
  if(isCapturing() == true)
    stopCapture();
  
  is_capturing = false;
  int camID = v_cam_bus->getInt();
  arv_update_device_list();
  int nDevices = arv_get_n_devices ();
  if(nDevices < camID) {
    cout << "Max ID is " << nDevices;
    #ifndef VDATA_NO_QT
      mutex.unlock();
    #endif
    return false;
  }
  
  dev_name = arv_get_device_id(camID);
  camera = arv_camera_new (dev_name);
  
  if (camera == NULL) {
    cout << "Could not connect to the camera: " << dev_name << endl;
    #ifndef VDATA_NO_QT
      mutex.unlock();
    #endif
    return false;
  }
  /* Set frame rate to 10 Hz */
  //TODO: Make frame rate configurable
  //arv_camera_set_frame_rate (camera, 45.0);
  
  //TODO: Make pixel format configurable
  arv_camera_set_pixel_format(camera, ARV_PIXEL_FORMAT_RGB_8_PACKED);
  
  /* retrieve image payload (number of bytes per image) */
  int payload = arv_camera_get_payload (camera);
  stream = arv_camera_create_stream (camera, NULL, NULL);
  if (stream == NULL) {
    cout << "Could not create stream for " << dev_name << endl;
    #ifndef VDATA_NO_QT
      mutex.unlock();
    #endif
    return false;
    
  }
  ArvPixelFormat pixel_format = arv_camera_get_pixel_format (camera);
  const char* pxString = arv_camera_get_pixel_format_as_string (camera);
  /* Push 50 buffer in the stream input buffer queue */
  for (int i = 0; i < 50; i++)
    arv_stream_push_buffer (stream, arv_buffer_new (payload, NULL));
  
  /* Start the video stream */
  arv_camera_start_acquisition (camera);

  /* Connect the new-buffer signal */
  //g_signal_connect (stream, "new-buffer", G_CALLBACK (new_buffer_cb), this);

  is_capturing = true;  
  
#ifndef VDATA_NO_QT
  mutex.unlock();
#endif
  return true;
}

bool CaptureGigE::copyFrame(const RawImage & src, RawImage & target)
{
    return convertFrame(src,target,src.getColorFormat());
}

bool CaptureGigE::copyAndConvertFrame(const RawImage & src, RawImage & target)
{
    return convertFrame(src, target, Colors::stringToColorFormat(v_colorout->getSelection().c_str()));
}

bool CaptureGigE::convertFrame(const RawImage & src, RawImage & target, ColorFormat output_fmt, int y16bits)
{
#ifndef VDATA_NO_QT
    mutex.lock();
#endif
    ColorFormat src_fmt=src.getColorFormat();
    if (target.getData()==0) {
        //allocate target, if it does not exist yet
        target.allocate(output_fmt,src.getWidth(),src.getHeight());
    } else {
        target.ensure_allocation(output_fmt,src.getWidth(),src.getHeight());
        //target.setWidth(src.getWidth());
        //target.setHeight(src.getHeight());
        //target.setFormat(output_fmt);
    }
    target.setTime(src.getTime());
    if (output_fmt==src_fmt) {
        //just do a memcpy
        memcpy(target.getData(),src.getData(),src.getNumBytes());
    } else {
        //do some more fancy conversion
        /*if ((src_fmt==COLOR_MONO8 || src_fmt==COLOR_RAW8) && output_fmt==COLOR_RGB8) {
            Conversions::y2rgb (src.getData(), target.getData(), width, height);
        } else if ((src_fmt==COLOR_MONO16 || src_fmt==COLOR_RAW16)) {
            if (output_fmt==COLOR_RGB8) {
                Conversions::y162rgb (src.getData(), target.getData(), width, height, y16bits);
            }
            else {
                fprintf(stderr,"Cannot copy and convert frame...unknown conversion selected from: %s to %s\n",
                        Colors::colorFormatToString(src_fmt).c_str(),
                        Colors::colorFormatToString(output_fmt).c_str());
#ifndef VDATA_NO_QT
                mutex.unlock();
#endif
                return false;
            }
        } else */ if (src_fmt==COLOR_RGB8 && output_fmt==COLOR_YUV422_UYVY) {
            Conversions::rgb2uyvy (src.getData(), target.getData(), width, height);
        } else if (src_fmt==COLOR_RGB8 && output_fmt==COLOR_YUV422_YUYV) {
            Conversions::rgb2yuyv (src.getData(), target.getData(), width, height);
        } else {
            fprintf(stderr,"Cannot copy and convert frame...unknown conversion selected from: %s to %s\n",
                    Colors::colorFormatToString(src_fmt).c_str(),
                    Colors::colorFormatToString(output_fmt).c_str());
    #ifndef VDATA_NO_QT
            mutex.unlock();
    #endif
            return false;
        }
    }
#ifndef VDATA_NO_QT
    mutex.unlock();
#endif
    return true;
}

RawImage CaptureGigE::getFrame()
{
#ifndef VDATA_NO_QT
    mutex.lock();
#endif
    
  RawImage result;
  fprintf(stderr, "CaptureVFL::getFrame %d", is_capturing);
  result.setColorFormat(COLOR_RGB8);
  result.setWidth(0);
  result.setHeight(0);
  //result.size= RawImage::computeImageSize(format, width*height);
  result.setTime(0.0);

  ArvBuffer *buffer;
  static int bufferCount = 0;
  //buffer = arv_stream_pop_buffer (stream);
  buffer = arv_stream_try_pop_buffer (stream);
  if (buffer != NULL) {
    
    if (arv_buffer_get_status (buffer) == ARV_BUFFER_STATUS_SUCCESS) {
      result.setWidth(arv_buffer_get_image_width(buffer));
      result.setHeight(arv_buffer_get_image_height(buffer));
  
      bufferCount++;
      unsigned char *buffer_data;
      size_t buffer_size;

      buffer_data = (unsigned char *) arv_buffer_get_data (buffer, &buffer_size);
      if(currentFrame == NULL) {
	currentFrame = new unsigned char[buffer_size];
      }
      memcpy(currentFrame, buffer_data, buffer_size);
      

      struct timeval tv;
      gettimeofday(&tv,NULL);
      result.setTime((double)tv.tv_sec + tv.tv_usec*(1.0E-6)); 
      result.setData(currentFrame);
    }
    arv_stream_push_buffer (stream, buffer);
  }  
  
  
#ifndef VDATA_NO_QT
    mutex.unlock();
#endif
    return rawFrame;
}

void CaptureGigE::releaseFrame() {
#ifndef VDATA_NO_QT
    mutex.lock();
#endif
    
    //frame management done at low-level now...
#ifndef VDATA_NO_QT
    mutex.unlock();
#endif
}

string CaptureGigE::getCaptureMethodName() const {
    return "V4L";
}


