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
//
//========================================================================
// Some portions of software interfacing with XIO originally writtten
// by James Bruce as part of the CMVision library.
//      http://www.cs.cmu.edu/~jbruce/cmvision/
// (C) 2004-2006 James R. Bruce, Carnegie Mellon University
// Licenced under the GNU General Public License (GPL) version 2,
//   or alternately by a specific written agreement.
//========================================================================
// (C) 2016 Eric Zavesky, Emoters
// Licenced under the GNU General Public License (GPL) version 3,
//   or alternately by a specific written agreement.
//========================================================================
/*!
 \file    capture_gige.h
 \brief   C++ Interface: CaptureGigE
 \author  Haresh Chudgar, (C) 2016
 */
//========================================================================

#ifndef CAPTUREGIGE_H
#define CAPTUREGIGE_H
#include "captureinterface.h"
#include "util.h"
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <arv.h>
//#include "VarTypes.h"


/*!
 \class CaptureGigE
 \brief Implements capture interface for gige vision cameras
 \author  Haresh Chudgar, (C) 2016
 
 If you find your camera not working correctly, or discover a bug,
 please inform the author, but also consider contributing yourself,
 so that we can cover as many options as possible.
 */

typedef struct {
    QMutex *mutex;
    unsigned char* imageBuffer;
    bool isNewBuffer;
    unsigned int width, height;
} ApplicationData;

#ifndef VDATA_NO_QT
#include <QMutex>
//if using QT, inherit QObject as a base
class CaptureGigE : public QObject, public CaptureInterface
#else
class CaptureGigE : public CaptureInterface
#endif
{
#ifndef VDATA_NO_QT
    Q_OBJECT
    public Q_SLOTS:
    void changed(VarType * group);
protected:
    QMutex mutex;
public:
#endif
    
protected:
  
    ApplicationData cbData;
    ArvCamera *camera;
    ArvStream* stream;
    const char* dev_name;
  
    void discoverDevices(char** deviceList, int* nDevices);
  
    unsigned char* currentFrame;

    bool is_capturing;
    
    //processing variables:
    VarStringEnum * v_colorout;
    
    //DCAM parameters:
    VarList * P_BRIGHTNESS;
    VarList * P_CONTRAST;
    VarList * P_EXPOSURE;
    VarList * P_SHARPNESS;
    VarList * P_WHITE_BALANCE;
    VarList * P_HUE;
    VarList * P_SATURATION;
    VarList * P_GAMMA;
    VarList * P_GAIN;
    VarList * P_TEMPERATURE;  // white balance temperature
    VarList * P_FRAME_RATE;
    
    //capture variables:
    VarInt    * v_cam_bus;
    VarInt    * v_fps;
    VarInt    * v_width;
    VarInt    * v_height;
    VarInt    * v_left;
    VarInt    * v_top;
    VarStringEnum * v_colormode;
    VarStringEnum * v_format;
    VarInt    * v_buffer_size;
    
    unsigned int cam_id;
    int width;
    int height;
    int top;
    int left;
    ColorFormat capture_format;
    int ring_buffer_size;
    int cam_count;
    RawImage rawFrame;
    unsigned int bufferCount;
    VarList * dcam_parameters;
    VarList * capture_settings;
    VarList * conversion_settings;
    
public:
#ifndef VDATA_NO_QT
    CaptureGigE(VarList * _settings=0,int default_camera_id=0,QObject * parent=0);
    void mvc_connect(VarList * group);
#else
    CaptureGigE(VarList * _settings=0,int default_camera_id=0);
#endif
    ~CaptureGigE();
    
    /// Initialize the interface and start capture
    virtual bool startCapture();
    
    /// Stop Capture
    virtual bool stopCapture();
    
    virtual bool isCapturing() { return is_capturing; };
    
    /// this gives a raw-image with a pointer directly to the video-buffer
    /// Note that this pointer is only guaranteed to point to a valid
    /// memory location until releaseFrame() is called.
    virtual RawImage getFrame();
    
    virtual void releaseFrame();
    
    virtual bool resetBus();
    
    void cleanup();
    
    void readParameterValues(VarList * item);
    
    void writeParameterValues(VarList * item);
    
    /// will perform a pure memcpy of an image, independent of vartypes
    /// conversion settings
    bool copyFrame(const RawImage & src, RawImage & target);
    
    virtual void readAllParameterValues();
    
    void writeAllParameterValues();
    
    virtual bool copyAndConvertFrame(const RawImage & src, RawImage & target);
    
    virtual string getCaptureMethodName() const;
    
protected:
    /// This function is used internally only
    /// The user should call copyAndConvertFrame with parameters setup through the
    /// VarTypes settings.
    bool convertFrame(const RawImage & src, RawImage & target,
                      ColorFormat output_fmt, int y16bits=16);
};

#endif
