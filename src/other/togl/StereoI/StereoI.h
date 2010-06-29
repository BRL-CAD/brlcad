/*----------------------------------------------------------------------------------------------------
Copyright NVIDIA Corporation 2005
TO THE MAXIMUM EXTENT PERMITTED BY APPLICABLE LAW, THIS SOFTWARE IS PROVIDED *AS IS* AND NVIDIA AND
AND ITS SUPPLIERS DISCLAIM ALL WARRANTIES, EITHER EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO,
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL NVIDIA
OR ITS SUPPLIERS BE LIABLE FOR ANY SPECIAL, INCIDENTAL, INDIRECT, OR CONSEQUENTIAL DAMAGES WHATSOEVER
INCLUDING, WITHOUT LIMITATION, DAMAGES FOR LOSS OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF
BUSINESS INFORMATION, OR ANY OTHER PECUNIARY LOSS) ARISING OUT OF THE USE OF OR INABILITY TO USE THIS
SOFTWARE, EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
----------------------------------------------------------------------------------------------------*/

#ifndef STEREO_I_H
#define STEREO_I_H

// Stereo states: bit 0 - on/off; bit 1 - enabled/disabled.
// Returned by GetStereoState.
#define STEREO_STATE_ENABLED         0x2
#define STEREO_STATE_DISABLED        0x0
#define STEREO_STATE_ON              0x1
#define STEREO_STATE_OFF             0x0

// CaptureImageFormats.
#define IMAGE_JPEG     0
#define IMAGE_PNG      1

// Image quality applies to JPEG only and varies from 0 to 100. 100 being the best.
// Default setting for CaptureStereoImage is JPEG and quality=75. 

interface StereoI
{
	virtual int			CheckAPIVersion(int);
    virtual int			GetStereoState(void);
	virtual int			SetStereoState(int);
	virtual float       GetSeparation(void);
    virtual float       SetSeparation(float);
    virtual float       GetConvergence(void);
    virtual float       SetConvergence(float);
	virtual void		CaptureStereoImage(int format, int quality);
	//Stereo images are dumped to [RootDir]\NVSTEREO.IMG
};

int CreateStereoI(StereoI **ppStereoI);
int isStereoEnabled(void);

#endif
