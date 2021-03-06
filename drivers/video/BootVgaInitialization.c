/*
 * video-related stuff
 * 2004-03-03  dmp@davidmpye.dyndns.org	Synced with kernel fb driver, added proper focus support etc
 * 2003-02-02  andy@warmcat.com  	Major reshuffle, threw out tons of unnecessary init
                                 	Made a good start on separating the video mode from the AV cable
				 	Consolidated init tables into a big struct (see boot.h)
 * 2002-12-04  andy@warmcat.com  Video now working :-)
 */

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "boot.h"
#include "BootEEPROM.h"
#include "config.h"
#include "BootVideo.h"
#include "video.h"
#include "memory_layout.h"
#include "VideoInitialization.h"
#include "BootVgaInitialization.h"
#include "encoder.h"
#include "xcalibur.h"

/*
void DetectVideoEncoder(void) {
	if (I2CTransmitByteGetReturn(0x45,0x000) != ERR_I2C_ERROR_BUS) video_encoder = ENCODER_CONEXANT;
	else video_encoder = ENCODER_FOCUS;
}
*/
void DetectVideoEncoder(void)
{
	unsigned int temp;

	// Default Value	
	video_encoder = ENCODER_CONEXANT;
	
	if (ReadfromSMBus(0x45,0x00,1,&temp) != ERR_I2C_ERROR_BUS) {
		// We successful got Data from the CONNEXANT CHIP
		video_encoder = ENCODER_CONEXANT;
	
	} else {
		// We did not get Data from Connexant
		
		if (ReadfromSMBus(0x6a,0x00,1,&temp) == ERR_I2C_ERROR_BUS) {
			// Focus Failed
			// XCAL video encoder detected
			video_encoder = ENCODER_XCALIBUR;
				
			
		} else {
			// Focus Detection OK
			video_encoder = ENCODER_FOCUS;
		}
	}
}

void XCaliburDumpNormalSequence(unsigned int *I2C_Encoder_Values)
{
	
	int i;
	ReadfromSMBus(0x70,4,4,&i);
	//wait_us(10);
	WriteToSMBus(0x70,4,4,0x0F000000);
	//wait_us(10);
	ReadfromSMBus(0x70,0,4,&i);
	//wait_us(10);
	WriteToSMBus(0x70,0,4,0x00000000);
	//wait_us(10);

	// von Register 0x01 bis 0x41
	for (i= 0; i< 64; i++) {
		WriteToSMBus(0x70,xcal_videregister_sequence[i],4,I2C_Encoder_Values[i]);
	//	wait_us(200);
	}
	
	ReadfromSMBus(0x70,0,4,&i);
	//wait_us(200);
	
	// von Register 0x00 bis 0x0e
	for (i= 64; i< 68; i++) {
		WriteToSMBus(0x70,xcal_videregister_sequence[i],4,I2C_Encoder_Values[i]);
	//	wait_us(200);
	}
	
	ReadfromSMBus(0x70,0x60,4,&i);
	//wait_us(200);

	// von Register 0x60 bis ende
	for (i= 68; i<(sizeof(xcal_videregister_sequence)); i++) {
		WriteToSMBus(0x70,xcal_videregister_sequence[i],4,I2C_Encoder_Values[i]);
	//	wait_us(200);
	}
	
	return ;
}




void BootVgaInitializationKernelNG(CURRENT_VIDEO_MODE_DETAILS * pcurrentvideomodedetails) {
	xbox_tv_encoding tv_encoding; 
	xbox_av_type av_type;
	BYTE b;
	RIVA_HW_INST riva;
        struct riva_regs newmode;
	int encoder_ok = 0;
	int i=0;
	GPU_PARAMETER gpu;
	xbox_video_mode encoder_mode;
	
	tv_encoding = DetectVideoStd();
	DetectVideoEncoder();
	
        // Dump to global variable
	VIDEO_AV_MODE=I2CTransmitByteGetReturn(0x10, 0x04);
	av_type = DetectAvType();
	gpu.av_type = av_type;

   	memset((void *)pcurrentvideomodedetails,0,sizeof(CURRENT_VIDEO_MODE_DETAILS));

	//Focus driver (presumably XLB also) doesnt do widescreen yet - only blackscreens otherwise.
	if(((BYTE *)&eeprom)[0x96]&0x01 && video_encoder == ENCODER_CONEXANT) { // 16:9 widescreen TV
		pcurrentvideomodedetails->m_nVideoModeIndex=VIDEO_MODE_1024x576;
	} else { // 4:3 TV
		pcurrentvideomodedetails->m_nVideoModeIndex=VIDEO_PREFERRED_MODE;
	}

	pcurrentvideomodedetails->m_pbBaseAddressVideo=(BYTE *)0xfd000000;
	pcurrentvideomodedetails->m_fForceEncoderLumaAndChromaToZeroInitially=1;

        // If the client hasn't set the frame buffer start address, assume
        // it should be at 4M from the end of RAM.

	pcurrentvideomodedetails->m_dwFrameBufferStart = FRAMEBUFFER_START;

        (*(unsigned int*)0xFD600800) = (FRAMEBUFFER_START & 0x0fffffff);

	pcurrentvideomodedetails->m_bAvPack=I2CTransmitByteGetReturn(0x10, 0x04);
	pcurrentvideomodedetails->m_pbBaseAddressVideo=(BYTE *)0xfd000000;
	pcurrentvideomodedetails->m_fForceEncoderLumaAndChromaToZeroInitially=1;
	pcurrentvideomodedetails->m_bBPP = 32;

	b=I2CTransmitByteGetReturn(0x54, 0x5A); // the eeprom defines the TV standard for the box

	// The values for hoc and voc are stolen from nvtv small mode

	if(b != 0x40) {
		pcurrentvideomodedetails->hoc = 13.44;
		pcurrentvideomodedetails->voc = 14.24;
	} else {
		pcurrentvideomodedetails->hoc = 15.11;
		pcurrentvideomodedetails->voc = 14.81;
	}
	pcurrentvideomodedetails->hoc /= 100.0;
	pcurrentvideomodedetails->voc /= 100.0;

	mapNvMem(&riva,pcurrentvideomodedetails->m_pbBaseAddressVideo);
	unlockCrtNv(&riva,0);

	if (xbox_ram == 128) {
		MMIO_H_OUT32(riva.PFB    ,0,0x200,0x03070103);
	} else {
		MMIO_H_OUT32(riva.PFB    ,0,0x200,0x03070003);
	}

	MMIO_H_OUT32 (riva.PCRTC, 0, 0x800, pcurrentvideomodedetails->m_dwFrameBufferStart);

	IoOutputByte(0x80d3, 5);  // Kill all video out using an ACPI control pin

	MMIO_H_OUT32(riva.PRAMDAC,0,0x880,0);
	MMIO_H_OUT32(riva.PRAMDAC,0,0x884,0x0);
	MMIO_H_OUT32(riva.PRAMDAC,0,0x888,0x0);
	MMIO_H_OUT32(riva.PRAMDAC,0,0x88c,0x10001000);
	MMIO_H_OUT32(riva.PRAMDAC,0,0x890,0x10000000);
	MMIO_H_OUT32(riva.PRAMDAC,0,0x894,0x10000000);
	MMIO_H_OUT32(riva.PRAMDAC,0,0x898,0x10000000);
	MMIO_H_OUT32(riva.PRAMDAC,0,0x89c,0x10000000);
	
	writeCrtNv (&riva, 0, 0x14, 0x00);
	writeCrtNv (&riva, 0, 0x17, 0xe3); // Set CRTC mode register
	writeCrtNv (&riva, 0, 0x19, 0x10); // ?
	writeCrtNv (&riva, 0, 0x1b, 0x05); // arbitration0
	writeCrtNv (&riva, 0, 0x22, 0xff); // ?
	writeCrtNv (&riva, 0, 0x33, 0x11); // ?
	
	if((av_type == AV_VGA) || (av_type == AV_VGA_SOG) || (av_type == AV_HDTV)) {
		unsigned char pll_int;
		if (av_type == AV_HDTV) {
			xbox_hdtv_mode hdtv_mode = HDTV_480p;
			//Only 480p supported at present
			/*if (video_mode->yres > 800) {
				hdtv_mode = HDTV_1080i;
			}
			else if (video_mode->yres > 600) {
				hdtv_mode = HDTV_720p;
			}*/
			
			// Settings for 720x480@60Hz (480p)
			pcurrentvideomodedetails->m_dwWidthInPixels=720;
			pcurrentvideomodedetails->m_dwHeightInLines=480;
			pcurrentvideomodedetails->m_dwMarginXInPixelsRecommended=0;
		 	pcurrentvideomodedetails->m_dwMarginYInLinesRecommended=0;
        	
			/* HDTV uses hardcoded settings for these - these are the
			 * correct ones for 480p */
			gpu.xres = pcurrentvideomodedetails->m_dwWidthInPixels;
	       		gpu.nvhstart = 738;
			gpu.nvhtotal = 858;
			gpu.yres = pcurrentvideomodedetails->m_dwHeightInLines;
			gpu.nvvstart = 489;
			gpu.nvvtotal = 525;
			gpu.pixelDepth = (32 + 1) / 8;
			gpu.crtchdispend = pcurrentvideomodedetails->m_dwWidthInPixels;
			gpu.crtcvstart = gpu.nvvstart;
			gpu.crtcvtotal = gpu.nvvtotal;
			
			pll_int = (unsigned char)((double)27027 * 6.0 / 13.5e3 + 0.5);
			if (video_encoder == ENCODER_CONEXANT)
				encoder_ok = conexant_calc_hdtv_mode(hdtv_mode, pll_int, newmode.encoder_mode);
			else if (video_encoder == ENCODER_FOCUS)
				encoder_ok = focus_calc_hdtv_mode(hdtv_mode, pll_int, newmode.encoder_mode);
		}
		else {
			//VGA or VGA_SOG
			// Settings for 800x600@56Hz, 35 kHz HSync
			pcurrentvideomodedetails->m_dwWidthInPixels=800;
			pcurrentvideomodedetails->m_dwHeightInLines=600;
			pcurrentvideomodedetails->m_dwMarginXInPixelsRecommended=20;
			pcurrentvideomodedetails->m_dwMarginYInLinesRecommended=20;
		
			gpu.xres = pcurrentvideomodedetails->m_dwWidthInPixels;
	       		gpu.nvhstart = 900;
			gpu.nvhtotal = 1028;
			gpu.yres = pcurrentvideomodedetails->m_dwHeightInLines;
			gpu.nvvstart = 614;
			gpu.nvvtotal = 630;
			gpu.pixelDepth = (32 + 1) / 8;
			gpu.crtchdispend = pcurrentvideomodedetails->m_dwWidthInPixels;
			gpu.crtcvstart = gpu.nvvstart;
			gpu.crtcvtotal = gpu.nvvtotal;
			pll_int = (unsigned char)((double)36000 * 6.0 / 13.5e3 + 0.5);
			
			if (video_encoder == ENCODER_CONEXANT)
				encoder_ok = conexant_calc_vga_mode(av_type, pll_int, newmode.encoder_mode);
			else if (video_encoder == ENCODER_FOCUS);
				//No focus VGA functions as yet
		}
	}
	else {	
	/* All other cable types - normal SDTV */
		switch(pcurrentvideomodedetails->m_nVideoModeIndex) {
			case VIDEO_MODE_640x480:
				pcurrentvideomodedetails->m_dwWidthInPixels=640;
				pcurrentvideomodedetails->m_dwHeightInLines=480;
				pcurrentvideomodedetails->m_dwMarginXInPixelsRecommended=0;
				pcurrentvideomodedetails->m_dwMarginYInLinesRecommended=0;
				break;
			case VIDEO_MODE_640x576:
				pcurrentvideomodedetails->m_dwWidthInPixels=640;
				pcurrentvideomodedetails->m_dwHeightInLines=576;
				pcurrentvideomodedetails->m_dwMarginXInPixelsRecommended=40; // pixels
				pcurrentvideomodedetails->m_dwMarginYInLinesRecommended=40; // lines
				break;
			case VIDEO_MODE_720x576:
				pcurrentvideomodedetails->m_dwWidthInPixels=720;
				pcurrentvideomodedetails->m_dwHeightInLines=576;
				pcurrentvideomodedetails->m_dwMarginXInPixelsRecommended=40; // pixels
				pcurrentvideomodedetails->m_dwMarginYInLinesRecommended=40; // lines
				break;
			case VIDEO_MODE_800x600: // 800x600
				pcurrentvideomodedetails->m_dwWidthInPixels=800;
				pcurrentvideomodedetails->m_dwHeightInLines=600;
				pcurrentvideomodedetails->m_dwMarginXInPixelsRecommended=20;
				pcurrentvideomodedetails->m_dwMarginYInLinesRecommended=20; // lines
				break;
			case VIDEO_MODE_1024x576: // 1024x576
				pcurrentvideomodedetails->m_dwWidthInPixels=1024;
				pcurrentvideomodedetails->m_dwHeightInLines=576;
				pcurrentvideomodedetails->m_dwMarginXInPixelsRecommended=20;
				pcurrentvideomodedetails->m_dwMarginYInLinesRecommended=20; // lines
				break;
		}	
		encoder_mode.xres = pcurrentvideomodedetails->m_dwWidthInPixels; 
		encoder_mode.yres = pcurrentvideomodedetails->m_dwHeightInLines;
		encoder_mode.tv_encoding = tv_encoding;
		encoder_mode.bpp = 32;
		encoder_mode.hoc = pcurrentvideomodedetails->hoc;
		encoder_mode.voc = pcurrentvideomodedetails->voc;
		encoder_mode.av_type = av_type;
		encoder_mode.tv_encoding = tv_encoding;

		if (video_encoder == ENCODER_CONEXANT) {
			encoder_ok = conexant_calc_mode(&encoder_mode, &newmode);
		}
		else if (video_encoder == ENCODER_FOCUS) {
			encoder_ok = focus_calc_mode(&encoder_mode, &newmode);
		}
		else printk("Error - unknown encoder type detected\n");
		
        	gpu.xres = pcurrentvideomodedetails->m_dwWidthInPixels;
	       	gpu.nvhstart = newmode.ext.hsyncstart;
		gpu.nvhtotal = newmode.ext.htotal;
		gpu.yres = pcurrentvideomodedetails->m_dwHeightInLines;
		gpu.nvvstart = newmode.ext.vsyncstart;
		gpu.nvvtotal = newmode.ext.vtotal;
		gpu.pixelDepth = (32 + 1) / 8;
		gpu.crtchdispend = pcurrentvideomodedetails->m_dwWidthInPixels;
		gpu.crtcvstart = newmode.ext.vsyncstart;
		gpu.crtcvtotal = newmode.ext.vtotal;
	}

	if (encoder_ok) {
		//Set up the GPU 
		SetGPURegister(&gpu, pcurrentvideomodedetails->m_pbBaseAddressVideo);
		//Load registers into chip
		if (video_encoder == ENCODER_CONEXANT) {
			int n1=0;
		   	I2CWriteBytetoRegister(0x45,0xc4, 0x00); // EN_OUT = 1
		        // Conexant init (starts at register 0x2e)
		        n1=0;
		        for(i=0x2e;i<0x100;i+=2) {
		        	switch(i) {
		        		case 0x6c: // reset
		          			I2CWriteBytetoRegister(0x45,i, newmode.encoder_mode[n1] & 0x7f);
	                          		break;
	                    		case 0xc4: // EN_OUT
						I2CWriteBytetoRegister(0x45,i, newmode.encoder_mode[n1] & 0xfe);
				 		break;
					case 0xb8: // autoconfig
						break;
					default:
						I2CWriteBytetoRegister(0x45,i, newmode.encoder_mode[n1]);
						break;
				}
				n1++;
				wait_us(800);	
			}
                	// Timing Reset
			b=I2CTransmitByteGetReturn(0x45,0x6c) & (0x7f);
			I2CWriteBytetoRegister(0x45, 0x6c, 0x80|b);
			b=I2CTransmitByteGetReturn(0x45,0xc4) & (0xfe);
			I2CWriteBytetoRegister(0x45, 0xc4, 0x01|b); // EN_OUT = 1
		}
		else if (video_encoder == ENCODER_FOCUS) {
	             	for (i=0; i<0xc4; ++i) {
                        	I2CWriteBytetoRegister(0x6a, i, newmode.encoder_mode[i]);
				wait_us(800);
               		}
		}
	}

        NVDisablePalette (&riva, 0);
	writeCrtNv (&riva, 0, 0x44, 0x03);
	NVInitGrSeq(&riva);
	writeCrtNv (&riva, 0, 0x44, 0x00);
	NVInitAttr(&riva,0);
	IoOutputByte(0x80d8, 4);  // ACPI IO thing seen in kernel, set to 4
	IoOutputByte(0x80d6, 5);  // ACPI IO thing seen in kernel, set to 4 or 5
	NVVertIntrEnabled (&riva,0);
	NVSetFBStart (&riva, 0, pcurrentvideomodedetails->m_dwFrameBufferStart);
	IoOutputByte(0x80d3, 4);  // ACPI IO video enable REQUIRED <-- particularly crucial to get composite out
	// We dimm the Video OFF - focus video is implicitly disabled.
	if (video_encoder == ENCODER_CONEXANT) {
		I2CTransmitWord(0x45, (0xa8<<8)|0);
		I2CTransmitWord(0x45, (0xaa<<8)|0);
		I2CTransmitWord(0x45, (0xac<<8)|0);
	}
	NVWriteSeq(&riva, 0x01, 0x01);  /* reenable display */
	if (video_encoder == ENCODER_CONEXANT) {
		I2CWriteBytetoRegister(0x45, 0xA8, 0x81);
		I2CWriteBytetoRegister(0x45, 0xAA, 0x49);
		I2CWriteBytetoRegister(0x45, 0xAC, 0x8C);
	}
	else if (video_encoder == ENCODER_FOCUS) {
	     	b = I2CTransmitByteGetReturn(0x6a,0x0c);
		b &= ~0x01;
		I2CWriteBytetoRegister(0x6a,0x0c,b);
		b = I2CTransmitByteGetReturn(0x6a,0x0d);
		I2CWriteBytetoRegister(0x6a,0x0d,b);
	}



/*
	XCALIBUR
if PAL

	XCaliburDumpNormalSequence(xcal_composite_pal);
	RAMDACRegValues = nvRAMDACRegValues_xcal_composite_pal;
	CRTRegsValues = nvCRTRegs_xcal_composite_pal;
	
if NTSC
	XCaliburDumpNormalSequence(xcal_composite_ntsc);	
	RAMDACRegValues = nvRAMDACRegValues_xcal_composite_ntsc;
	CRTRegsValues = nvCRTRegs_xcal_composite_ntsc;
	
Dump Regions	
	for(i = 0; i < (sizeof(nvMainRegs) / 4); i++) {
		MMIO_H_OUT32(riva->PMC, 0, nvMainRegs[i], RAMDACRegValues[i]);
	}	

	// nVidia CRTC registers
	for(i = 0; i < sizeof(nvCRTRegs_xcal_composite_ntsc); i++) {
		writeCrtNv(riva, 0, i, CRTRegsValues[i]);
	}
	
NOTE: 648*480	

*/



}


static void NVSetFBStart (RIVA_HW_INST *riva, int head, DWORD dwFBStart) {
       MMIO_H_OUT32 (riva->PCRTC, head, 0x8000, dwFBStart);
       MMIO_H_OUT32 (riva->PMC, head, 0x8000, dwFBStart);
}

static void NVVertIntrEnabled (RIVA_HW_INST *riva, int head)
{
        MMIO_H_OUT32 (riva->PCRTC, head, 0x140, 0x1);
        MMIO_H_OUT32 (riva->PCRTC, head, 0x100, 0x1);
        MMIO_H_OUT32 (riva->PCRTC, head, 0x140, 1);
        MMIO_H_OUT32 (riva->PMC, head, 0x140, 0x1);
        MMIO_H_OUT32 (riva->PMC, head, 0x100, 0x1);
        MMIO_H_OUT32 (riva->PMC, head, 0x140, 1);
}

static inline void unlockCrtNv (RIVA_HW_INST *riva, int head)
{
	writeCrtNv (riva, head, 0x1f, 0x57); /* unlock extended registers */
}

static inline void lockCrtNv (RIVA_HW_INST *riva, int head)
{
	writeCrtNv (riva, head, 0x1f, 0x99); /* lock extended registers */
}


static void writeCrtNv (RIVA_HW_INST *riva, int head, int reg, BYTE val)
{
        VGA_WR08(riva->PCIO, CRT_INDEX(head), reg);
        VGA_WR08(riva->PCIO, CRT_DATA(head), val);
}

static void mapNvMem (RIVA_HW_INST *riva, BYTE *IOAddress)
{
	riva->PMC     = IOAddress+0x000000;
	riva->PFB     = IOAddress+0x100000;
	riva->PEXTDEV = IOAddress+0x101000;
	riva->PCRTC   = IOAddress+0x600000;
	riva->PCIO    = riva->PCRTC + 0x1000;
	riva->PVIO    = IOAddress+0x0C0000;
	riva->PRAMDAC = IOAddress+0x680000;
	riva->PDIO    = riva->PRAMDAC + 0x1000;
	riva->PVIDEO  = IOAddress+0x008000;
	riva->PTIMER  = IOAddress+0x009000;
}

static void NVDisablePalette (RIVA_HW_INST *riva, int head)
{
	volatile CARD8 tmp;
	tmp = VGA_RD08(riva->PCIO + head * HEAD, VGA_IOBASE_COLOR + VGA_IN_STAT_1_OFFSET);
	VGA_WR08(riva->PCIO + head * HEAD, VGA_ATTR_INDEX, 0x20);
}


static void NVWriteSeq(RIVA_HW_INST *riva, CARD8 index, CARD8 value)
{

        VGA_WR08(riva->PVIO, VGA_SEQ_INDEX, index);
		        VGA_WR08(riva->PVIO, VGA_SEQ_DATA,  value);

}

static void NVWriteGr(RIVA_HW_INST *riva, CARD8 index, CARD8 value)
{
        VGA_WR08(riva->PVIO, VGA_GRAPH_INDEX, index);
        VGA_WR08(riva->PVIO, VGA_GRAPH_DATA,  value);
}

static void NVInitGrSeq (RIVA_HW_INST *riva)
{
        NVWriteSeq(riva, 0x00, 0x03);
        NVWriteSeq(riva, 0x01, 0x21);
       	NVWriteSeq(riva, 0x02, 0x0f);
       	NVWriteSeq(riva, 0x03, 0x00);
       	NVWriteSeq(riva, 0x04, 0x06);
       	NVWriteGr(riva, 0x00, 0x00);
       	NVWriteGr(riva, 0x01, 0x00);
       	NVWriteGr(riva, 0x02, 0x00);
      	NVWriteGr(riva, 0x03, 0x00);
      	NVWriteGr(riva, 0x04, 0x00);    /* depth != 1 */
     	NVWriteGr(riva, 0x05, 0x40);    /* depth != 1 && depth != 4 */
     	NVWriteGr(riva, 0x06, 0x05);
      	NVWriteGr(riva, 0x07, 0x0f);
    	NVWriteGr(riva, 0x08, 0xff);
}

static void NVWriteAttr(RIVA_HW_INST *riva, int head, CARD8 index, CARD8 value)
{
 	MMIO_H_OUT8(riva->PCIO, head, VGA_ATTR_INDEX,  index);
        MMIO_H_OUT8(riva->PCIO, head, VGA_ATTR_DATA_W, value);
}

static void NVInitAttr (RIVA_HW_INST *riva, int head)
{
        NVWriteAttr(riva,0, 0, 0x01);
        NVWriteAttr(riva,0, 1, 0x02);
        NVWriteAttr(riva,0, 2, 0x03);
        NVWriteAttr(riva,0, 3, 0x04);
        NVWriteAttr(riva,0, 4, 0x05);
        NVWriteAttr(riva,0, 5, 0x06);
       	NVWriteAttr(riva,0, 6, 0x07);
        NVWriteAttr(riva,0, 7, 0x08);
        NVWriteAttr(riva,0, 8, 0x09);
       	NVWriteAttr(riva,0, 9, 0x0a);
        NVWriteAttr(riva,0, 10, 0x0b);
        NVWriteAttr(riva,0, 11, 0x0c);
      	NVWriteAttr(riva,0, 12, 0x0d);
     	NVWriteAttr(riva,0, 13, 0x0e);
     	NVWriteAttr(riva,0, 14, 0x0f);
        NVWriteAttr(riva,0, 15, 0x01);
       	NVWriteAttr(riva,0, 16, 0x4a);
        NVWriteAttr(riva,0, 17, 0x0f);
      	NVWriteAttr(riva,0, 18, 0x00);
      	NVWriteAttr(riva,0, 19, 0x00);
}

