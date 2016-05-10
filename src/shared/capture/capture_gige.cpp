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

#ifndef VDATA_NO_QT
CaptureGigE::CaptureGigE(VarList * _settings,int default_camera_id, QObject * parent) : QObject(parent), camera(NULL), CaptureInterface(_settings)
#else
CaptureGigE::CaptureGigE(VarList * _settings,int default_camera_id) : CaptureInterface(_settings)
#endif
{
#ifndef VDATA_NO_QT
    mutex.lock();
#endif
  settings->addChild(conversion_settings = new VarList("Conversion Settings"));
  settings->addChild(capture_settings = new VarList("Capture Settings"));
  settings->addChild(camera_parameters  = new VarList("Camera Parameters"));
  
  //=======================CONVERSION SETTINGS=======================
  conversion_settings->addChild(v_colorout=new VarStringEnum("convert to mode",Colors::colorFormatToString(COLOR_RGB8)));
  v_colorout->addItem(Colors::colorFormatToString(COLOR_RGB8));
  
  //=======================CAPTURE SETTINGS==========================
  //TODO: set default camera id
  capture_settings->addChild(v_cam_bus          = new VarInt("cam idx",0));
  capture_settings->addChild(v_fps              = new VarInt("framerate",3));
  capture_settings->addChild(v_width            = new VarInt("width",640));
  capture_settings->addChild(v_height           = new VarInt("height",480));
  capture_settings->addChild(v_left            = new VarInt("left",0));
  capture_settings->addChild(v_top           = new VarInt("top",0));
  capture_settings->addChild(v_colormode        = new VarStringEnum("capture mode",Colors::colorFormatToString(COLOR_RGB8)));
  v_colormode->addItem(Colors::colorFormatToString(COLOR_RGB8));
  capture_settings->addChild(v_buffer_size      = new VarInt("Buffer Queue size",50));
  //v_buffer_size->addFlags(VARTYPE_FLAG_READONLY);
  
  camera_parameters->addFlags( VARTYPE_FLAG_HIDE_CHILDREN );
  
  camera_parameters->addChild(P_EXPOSURE = new VarList("exposure"));
  P_EXPOSURE->addChild(new VarBool("enabled"));
  P_EXPOSURE->addChild(new VarBool("auto"));
  P_EXPOSURE->addChild(new VarTrigger("one-push","Auto!"));
  P_EXPOSURE->addChild(new VarInt("value"));
  P_EXPOSURE->addChild(new VarBool("use absolute"));
  P_EXPOSURE->addChild(new VarDouble("absolute value"));
  #ifndef VDATA_NO_QT
    mvc_connect(P_EXPOSURE);
  #endif
  
  
  camera_parameters->addChild(P_BRIGHTNESS = new VarList("brightness"));
  P_BRIGHTNESS->addChild(new VarBool("enabled"));
  P_BRIGHTNESS->addChild(new VarBool("default"));
  P_BRIGHTNESS->addChild(new VarInt("value"));
  
  camera_parameters->addChild(P_FRAME_RATE = new VarList("frame rate"));
  P_FRAME_RATE->addChild(new VarBool("enabled"));
  P_FRAME_RATE->addChild(new VarInt("value"));
  P_FRAME_RATE->addChild(new VarDouble("absolute value"));
  #ifndef VDATA_NO_QT
    mvc_connect(P_FRAME_RATE);
  #endif
    
  is_capturing = false;
  currentFrame = NULL;
#ifndef VDATA_NO_QT
    mutex.unlock();
#endif
}

int CaptureGigE::getNoDevices() {
  arv_update_device_list();
  return arv_get_n_devices();
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
  vector<VarType *> v=camera_parameters->getChildren();
  for (unsigned int i=0;i<v.size();i++) {
    if (v[i]->getType()==VARTYPE_ID_LIST) {
      readParameterValues((VarList *)v[i]);
    }
  }
}

void CaptureGigE::writeAllParameterValues() {
  vector<VarType *> v=camera_parameters->getChildren();
  for (unsigned int i=0;i<v.size();i++) {
    if (v[i]->getType()==VARTYPE_ID_LIST) {
      writeParameterValues((VarList *)v[i]);
    }
  }
}

void CaptureGigE::readParameterValues(VarList * item) {
  if(camera == NULL) {
    return;
  }
  vector<VarType *> children=item->getChildren();
  
  VarDouble * vabs=0;
  VarInt * vint=0;
  VarBool * vuseabs=0;
  VarBool * venabled=0;
  VarBool * vauto=0;
  VarBool * vwasread=0;
  VarTrigger * vtrigger=0;
  VarInt * vint2=0;
  VarInt * vint3=0;
  
  for (unsigned int i=0;i<children.size();i++) {
    if (children[i]->getType()==VARTYPE_ID_BOOL && children[i]->getName()=="was_read") vwasread=(VarBool *)children[i];
    if (children[i]->getType()==VARTYPE_ID_BOOL && children[i]->getName()=="enabled") venabled=(VarBool *)children[i];
    if (children[i]->getType()==VARTYPE_ID_BOOL && children[i]->getName()=="auto") vauto=(VarBool *)children[i];
    if (children[i]->getType()==VARTYPE_ID_BOOL && children[i]->getName()=="use absolute") vuseabs=(VarBool *)children[i];
    if (children[i]->getType()==VARTYPE_ID_INT && children[i]->getName()=="value") vint=(VarInt *)children[i];
    if (children[i]->getType()==VARTYPE_ID_DOUBLE && children[i]->getName()=="absolute value") vabs=(VarDouble *)children[i];
    if (children[i]->getType()==VARTYPE_ID_TRIGGER && children[i]->getName()=="one-push") vtrigger=(VarTrigger *)children[i];
  }
  
  if(item == P_EXPOSURE) {
    if(arv_camera_is_exposure_time_available(camera) == false) {
      return;
    }
    camera_parameters->removeFlags(VARTYPE_FLAG_HIDE_CHILDREN );
    vector<VarType *> children=item->getChildren();

    if (vwasread!=0) vwasread->setBool(false);
    ArvAuto isAuto = arv_camera_get_exposure_time_auto(camera);
    /* ArvAuto:
    *  @ARV_AUTO_OFF: manual setting
    *  @ARV_AUTO_ONCE: automatic setting done once, then returns to manual
    *  @ARV_AUTO_CONTINUOUS: setting is adjusted continuously
    */
    if (isAuto==ARV_AUTO_CONTINUOUS) {
      vauto->setBool(true);
    } else if (isAuto==ARV_AUTO_OFF || isAuto==ARV_AUTO_ONCE) {
      vauto->setBool(false);
    }
    vuseabs->setBool(true);
    vabs->setDouble(arv_camera_get_exposure_time(camera));
    vuseabs->removeFlags( VARTYPE_FLAG_READONLY);
    venabled->setBool(true);
  } else if(item == P_FRAME_RATE) {
    camera_parameters->removeFlags(VARTYPE_FLAG_HIDE_CHILDREN );
    if (vwasread!=0) vwasread->setBool(false);
    vabs->setDouble(arv_camera_get_frame_rate(camera));
    venabled->setBool(true);
  }
}

void CaptureGigE::writeParameterValues(VarList * item) {
  if(camera == NULL)
    return;
  
  #ifndef VDATA_NO_QT
    mutex.lock();
  #endif
  dc1394bool_t dcb;
  
  VarDouble * vabs=0;
  VarInt * vint=0;
  VarBool * vuseabs=0;
  VarBool * venabled=0;
  VarBool * vauto=0;
  VarInt * vint2=0;
  VarInt * vint3=0;
  VarBool * vwasread=0;
  VarTrigger * vtrigger=0;
  
  vector<VarType *> children=item->getChildren();
  for (unsigned int i=0;i<children.size();i++) {
    if (children[i]->getType()==VARTYPE_ID_BOOL && children[i]->getName()=="was_read") vwasread=(VarBool *)children[i];
    if (children[i]->getType()==VARTYPE_ID_BOOL && children[i]->getName()=="enabled") venabled=(VarBool *)children[i];
    if (children[i]->getType()==VARTYPE_ID_BOOL && children[i]->getName()=="auto") vauto=(VarBool *)children[i];
    if (children[i]->getType()==VARTYPE_ID_BOOL && children[i]->getName()=="use absolute") vuseabs=(VarBool *)children[i];
    if (children[i]->getType()==VARTYPE_ID_INT && children[i]->getName()=="value") vint=(VarInt *)children[i];
    if (children[i]->getType()==VARTYPE_ID_DOUBLE && children[i]->getName()=="absolute value") vabs=(VarDouble *)children[i];
    if (children[i]->getType()==VARTYPE_ID_TRIGGER && children[i]->getName()=="one-push") vtrigger=(VarTrigger *)children[i];
  }
  
  if(item == P_EXPOSURE) {
    if(arv_camera_is_exposure_time_available(camera) == false) {
      goto EXIT_WRITE;
    }
    if (vtrigger!=0 && vtrigger->getCounter() > 0) {
      vtrigger->resetCounter();
      printf("ONE PUSH for %s!\n",item->getName().c_str());
      arv_camera_set_exposure_time_auto(camera, ARV_AUTO_ONCE);
    } else {
      arv_camera_set_exposure_time_auto(camera, vauto->getBool() ? ARV_AUTO_CONTINUOUS : ARV_AUTO_OFF);
      if (vauto->getBool()==false) {
	arv_camera_set_exposure_time(camera, vabs->getDouble());
      }
    }
  } 
  
  if(item == P_FRAME_RATE) {
    bool isAvail = arv_camera_is_frame_rate_available(camera);
    fprintf(stderr, "IsAvail: %d\n", isAvail);
    if(isAvail == false) {
      goto EXIT_WRITE;
    }
    double reqFrameRate = vabs->getDouble();
    double minFR, maxFR;
    arv_camera_get_frame_rate_bounds(camera, &minFR, &maxFR);
    fprintf(stderr, "Min: %f, Max: %f, Req:%f\n", minFR, maxFR, reqFrameRate);
    if(reqFrameRate < minFR || reqFrameRate > maxFR) {
      goto EXIT_WRITE;
    }
    arv_camera_set_frame_rate(camera, reqFrameRate);
  }
  
  EXIT_WRITE:
#ifndef VDATA_NO_QT
    mutex.unlock();
#endif
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

static void
new_buffer_cb (ArvStream *stream, ApplicationData* data)
{
	ArvBuffer *buffer;
	buffer = arv_stream_try_pop_buffer (stream);
	if (buffer != NULL) {
		if (arv_buffer_get_status (buffer) == ARV_BUFFER_STATUS_SUCCESS) {
		  bool isLock = data->mutex->tryLock();
		  if(isLock == true) {
		    data->isNewBuffer = true;
		    size_t bufferSize;
		    unsigned char* imageBuffer = (unsigned char*) arv_buffer_get_data (buffer, &bufferSize);
		    memcpy(data->imageBuffer, imageBuffer, bufferSize);
		    data->width = arv_buffer_get_image_width(buffer);
		    data->height = arv_buffer_get_image_height(buffer);
		    data->mutex->unlock();
		  }
		}
			
		/* Image processing here */
		arv_stream_push_buffer (stream, buffer);
	}
}

static void control_lost_cb (ArvGvDevice *gv_device)
{
  //TODO: Handle this
  /* Control of the device is lost. Display a message and force application exit */
  fprintf(stderr, "Control lost\n");
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
  cbData.isNewBuffer = false;
  delete[] cbData.imageBuffer;
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
  arv_update_device_list();
  const char* deviceID = arv_get_device_id(v_cam_bus->getInt());
  fprintf(stderr, "Connecting to %s\n", deviceID);
  camera = arv_camera_new (deviceID);
  
  if (camera == NULL) {
    fprintf(stderr, "Could not connect to the camera: %s\n", deviceID);
    #ifndef VDATA_NO_QT
      mutex.unlock();
    #endif
    return false;
  }
  
  //arv_camera_set_region (camera, 0, 0, v_width->getInt(), v_height->getInt());
  width = v_width->get();
  height = v_height->get();
  int payload = arv_camera_get_payload (camera);
  fprintf(stderr, "payload: %d, width: %d, height: %d", payload, v_width->getInt(), v_height->getInt());
  /* retrieve image payload (number of bytes per image) */
  stream = arv_camera_create_stream (camera, NULL, NULL);
  if (stream == NULL) {
    g_object_unref(camera);
    camera = NULL;
    fprintf(stderr, "Could not create stream for %s\n", deviceID);
    #ifndef VDATA_NO_QT
      mutex.unlock();
    #endif
    return false;
  }
  
  /* Push 50 buffer in the stream input buffer queue */
  for (int i = 0; i < v_buffer_size->getInt(); i++)
    arv_stream_push_buffer (stream, arv_buffer_new (payload, NULL));
  
  /* Start the video stream */
  arv_camera_start_acquisition (camera);
  
  cbData.mutex = &mutex;
  cbData.imageBuffer = new unsigned char[payload];
  cbData.isNewBuffer = false;
  /* Connect the new-buffer signal */
  g_signal_connect (stream, "new-buffer", G_CALLBACK (new_buffer_cb), &cbData);
  /* And enable emission of this signal (it's disabled by default for performance reason) */
  arv_stream_set_emit_signals (stream, TRUE);

  /* Connect the control-lost signal */
  g_signal_connect (arv_camera_get_device (camera), "control-lost", G_CALLBACK (control_lost_cb), NULL);
  
  is_capturing = true;  
  
#ifndef VDATA_NO_QT
  mutex.unlock();
#endif
  fprintf(stderr, "%s started\n", deviceID);
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
        if ((src_fmt==COLOR_MONO8 || src_fmt==COLOR_RAW8) && output_fmt==COLOR_RGB8) {
	    dc1394_bayer_decoding_8bit( src.getData(), target.getData(), src.getWidth(), src.getHeight(), DC1394_COLOR_FILTER_RGGB, DC1394_BAYER_METHOD_NEAREST);
            //Conversions::y2rgb (src.getData(), target.getData(), width, height);
        } else if ((src_fmt==COLOR_MONO16 || src_fmt==COLOR_RAW16)) {
            if (output_fmt==COLOR_RGB8) {
                Conversions::y162rgb (src.getData(), target.getData(), width, height, y16bits);
            }
            else {
	      /* Haresh
                fprintf(stderr,"Cannot copy and convert frame...unknown conversion selected from: %s to %s\n",
                        Colors::colorFormatToString(src_fmt).c_str(),
                        Colors::colorFormatToString(output_fmt).c_str());
              */
#ifndef VDATA_NO_QT
                mutex.unlock();
#endif
                return false;
            }
        } else if (src_fmt==COLOR_RGB8 && output_fmt==COLOR_YUV422_UYVY) {
            Conversions::rgb2uyvy (src.getData(), target.getData(), width, height);
        } else if (src_fmt==COLOR_RGB8 && output_fmt==COLOR_YUV422_YUYV) {
            Conversions::rgb2yuyv (src.getData(), target.getData(), width, height);
        } else {
	  /* Haresh
            fprintf(stderr,"Cannot copy and convert frame...unknown conversion selected from: %s to %s\n",
                    Colors::colorFormatToString(src_fmt).c_str(),
                    Colors::colorFormatToString(output_fmt).c_str());
                    */
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
  //fprintf(stderr, "Received full buffer %d\n", cbData.buffer_count);
  if(cbData.isNewBuffer == true){
      cbData.isNewBuffer = false;
      result.setColorFormat(COLOR_RAW8);
      result.setData(cbData.imageBuffer);
      struct timeval tv;
      gettimeofday(&tv,NULL);
      result.setTime((double)tv.tv_sec + tv.tv_usec*(1.0E-6));
      result.setHeight(cbData.height);
      result.setWidth(cbData.width);
  }
#ifndef VDATA_NO_QT
    mutex.unlock();
#endif
  return result;
}

void CaptureGigE::releaseFrame() {
  //frame management done at low-level now...
  return;
  
#ifndef VDATA_NO_QT
    mutex.lock();
#endif
    
#ifndef VDATA_NO_QT
    mutex.unlock();
#endif
}

string CaptureGigE::getCaptureMethodName() const {
    return "GigE";
}


