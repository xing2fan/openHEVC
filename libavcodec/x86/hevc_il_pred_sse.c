#include "config.h"
#include "libavutil/avassert.h"
#include "libavutil/pixdesc.h"
#include "libavcodec/get_bits.h"
#include "libavcodec/hevc.h"
#include "libavcodec/x86/hevcdsp.h"
#include "libavcodec/hevc_up_sample_filter.h"
#include "libavcodec/bit_depth_template.c"





#include <emmintrin.h>
#include <tmmintrin.h>
#include <smmintrin.h>

#define BIT_DEPTH 8

#define LumHor_FILTER(pel, coeff) \
(pel[0]*coeff[0] + pel[1]*coeff[1] + pel[2]*coeff[2] + coeff[3]*pel[3] + pel[4]*coeff[4] + pel[5]*coeff[5] + pel[6]*coeff[6] + pel[7]*coeff[7])

#define CroHor_FILTER(pel, coeff) \
(pel[0]*coeff[0] + pel[1]*coeff[1] + pel[2]*coeff[2] + pel[3]*coeff[3])

#define LumVer_FILTER(pel, coeff) \
(pel[0]*coeff[0] + pel[1]*coeff[1] + pel[2]*coeff[2] + pel[3]*coeff[3] + pel[4]*coeff[4] + pel[5]*coeff[5] + pel[6]*coeff[6] + pel[7]*coeff[7])

#define CroVer_FILTER(pel, coeff) \
(pel[0]*coeff[0] + pel[1]*coeff[1] + pel[2]*coeff[2] + pel[3]*coeff[3])

#define LumVer_FILTER1(pel, coeff, width) \
(pel[0]*coeff[0] + pel[width]*coeff[1] + pel[width*2]*coeff[2] + pel[width*3]*coeff[3] + pel[width*4]*coeff[4] + pel[width*5]*coeff[5] + pel[width*6]*coeff[6] + pel[width*7]*coeff[7])
// Define the function for up-sampling
#define CroVer_FILTER1(pel, coeff, widthEL) \
(pel[0]*coeff[0] + pel[widthEL]*coeff[1] + pel[widthEL*2]*coeff[2] + pel[widthEL*3]*coeff[3])

#ifdef  SVC_EXTENSION

void ff_upsample_base_layer_frame_sse_h(struct AVFrame *FrameEL, struct AVFrame *FrameBL, short *Buffer[3], const int32_t enabled_up_sample_filter_luma[16][8], const int32_t enabled_up_sample_filter_chroma[16][4], struct HEVCWindow *Enhscal, struct UpsamplInf *up_info, int channel)
{
    
    int widthBL =  FrameBL->coded_width;
    int heightBL = FrameBL->coded_height;
    int strideBL = FrameBL->linesize[0];
    
    int widthEL =  FrameEL->width - Enhscal->left_offset - Enhscal->right_offset;
    int heightEL = FrameEL->height - Enhscal->top_offset - Enhscal->bottom_offset;
    int strideEL = FrameEL->linesize[0];
    
    
    int CTB = 64;
    int refPos16 = 0;
    int phase    = 0;
    int refPos   = 0;
    
    widthEL   = FrameEL->width;  //pcUsPic->getWidth ();
    heightEL  = FrameEL->height; //pcUsPic->getHeight();
    
    widthBL   = FrameBL->coded_width;
    heightBL  = FrameBL->coded_height <= heightEL ? FrameBL->coded_height:heightEL;  // min( FrameBL->height, heightEL);
    
    int start = channel * CTB;
    heightBL = heightBL <= ((channel+1) * CTB) ? heightBL:((channel+1) * CTB);
    int leftStartL = Enhscal->left_offset;
    int rightEndL  = FrameEL->width - Enhscal->right_offset;

    if(1 ) {
        const int32_t* coeff;
        int i,j;
        uint8_t buffer[8];
        uint8_t *srcBufY = FrameBL->data[0]+(start*strideBL);
        
        short *tempBufY = Buffer[0]+(start*widthEL);
        uint8_t *srcY;
        short *dstY1;
        for( i = 0; i < widthEL; i++ )	{
            int x = av_clip(i, leftStartL, rightEndL);
            refPos16 = (((x - leftStartL)*up_info->scaleXLum + up_info->addXLum) >> 12);
            phase    = refPos16 & 15;
            refPos   = refPos16 >> 4;
            coeff = enabled_up_sample_filter_luma[phase];
            refPos -= ((NTAPS_LUMA>>1) - 1);
            srcY = srcBufY + refPos;
            dstY1 = tempBufY + i;
            if(refPos < 0)
                for( j = start; j < heightBL ; j++ )	{
                    
                    memset(buffer, srcY[-refPos], -refPos);
                    memcpy(buffer-refPos, srcY-refPos, 8+refPos);
                    *dstY1 = LumHor_FILTER(buffer, coeff);
                    
                    srcY += strideBL;
                    dstY1 += widthEL;//strideEL;
                }else if(refPos+8 > widthBL )
                    for( j = start; j < heightBL ; j++ )	{
                        
                        memcpy(buffer, srcY, widthBL-refPos);
                        memset(buffer+(widthBL-refPos), srcY[widthBL-refPos-1], 8-(widthBL-refPos));
                        *dstY1 = LumHor_FILTER(buffer, coeff);
                        
                        srcY += strideBL;
                        dstY1 += widthEL;//strideEL;
                    }else
                        for( j = start; j < heightBL ; j++ )	{
                            
                            *dstY1 = LumHor_FILTER(srcY, coeff);
                            srcY += strideBL;
                            dstY1 += widthEL;//strideEL;
                        }
        }
    }
    if( 1 ) {
        widthBL   = FrameBL->coded_width;
        heightBL  = FrameBL->coded_height;
        
        widthEL   = FrameEL->width - Enhscal->right_offset - Enhscal->left_offset;
        heightEL  = FrameEL->height - Enhscal->top_offset - Enhscal->bottom_offset;
        
        widthEL  >>= 1;
        heightEL >>= 1;
        widthBL  >>= 1;
        heightBL >>= 1;
        
        int leftStartC = Enhscal->left_offset>>1;
        int rightEndC  = (FrameEL->width>>1) - (Enhscal->right_offset>>1);
        
        
        
        widthEL   = FrameEL->width >> 1;
        heightEL  = FrameEL->height >> 1;
        widthBL   = FrameBL->coded_width >> 1;
        heightBL  = FrameBL->coded_height > heightEL ? FrameBL->coded_height:heightEL;
        
        
        
        heightBL >>= 1;
        
        
        start = channel * CTB/2;

        const int32_t* coeff;
        int i,j;
        uint8_t buffer[8];
        strideBL  = FrameBL->linesize[1];
        strideEL  = FrameEL->linesize[1];
        
        
        uint8_t *srcBufU = FrameBL->data[1]+(start*strideBL);
        
        short *tempBufU = Buffer[1]+(start*widthEL);
        uint8_t *srcU;
        short *dstU1;
        
        uint8_t *srcBufV = FrameBL->data[2]+(start*strideBL);
        short *tempBufV = Buffer[2]+(start*widthEL);
        uint8_t *srcV;
        
        short *dstV1;
        
        
        
        heightBL = heightBL <= ((channel+1) * CTB/2) ? heightBL:((channel+1) * CTB/2);
        
        
        //========== horizontal upsampling ===========
        for( i = 0; i < widthEL; i++ )	{
            int x = av_clip(i, leftStartC, rightEndC - 1);
            refPos16 = (((x - leftStartC)*up_info->scaleXCr + up_info->addXCr) >> 12);
            phase    = refPos16 & 15;
            refPos   = refPos16 >> 4;
            coeff = enabled_up_sample_filter_chroma[phase];
            
            refPos -= ((NTAPS_CHROMA>>1) - 1);
            srcU = srcBufU + refPos; // -((NTAPS_CHROMA>>1) - 1);
            srcV = srcBufV + refPos; // -((NTAPS_CHROMA>>1) - 1);
            dstU1 = tempBufU + i;
            dstV1 = tempBufV + i;
            
            if(refPos < 0)
                for( j = start; j < heightBL ; j++ )	{
                    
                    memset(buffer, srcU[-refPos], -refPos);
                    memcpy(buffer-refPos, srcU-refPos, 4+refPos);
                    memset(buffer+4, srcV[-refPos], -refPos);
                    memcpy(buffer-refPos+4, srcV-refPos, 4+refPos);
                    
                    *dstU1 = CroHor_FILTER(buffer, coeff);
                    
                    *dstV1 = CroHor_FILTER((buffer+4), coeff);
                    
                    
                    srcU += strideBL;
                    srcV += strideBL;
                    dstU1 += widthEL;
                    dstV1 += widthEL;
                }else if(refPos+4 > widthBL )
                    for( j = start; j < heightBL ; j++ )	{
                        
                        memcpy(buffer, srcU, widthBL-refPos);
                        memset(buffer+(widthBL-refPos), srcU[widthBL-refPos-1], 4-(widthBL-refPos));
                        
                        memcpy(buffer+4, srcV, widthBL-refPos);
                        memset(buffer+(widthBL-refPos)+4, srcV[widthBL-refPos-1], 4-(widthBL-refPos));
                        
                        *dstU1 = CroHor_FILTER(buffer, coeff);
                        
                        *dstV1 = CroHor_FILTER((buffer+4), coeff);
                        
                        srcU += strideBL;
                        srcV += strideBL;
                        dstU1 += widthEL;
                        dstV1 += widthEL;
                    }else
                        for( j = start; j < heightBL ; j++ )	{
                            
                            *dstU1 = CroHor_FILTER(srcU, coeff);
                            
                            *dstV1 = CroHor_FILTER(srcV, coeff);
                            
                            
                            srcU += strideBL;
                            srcV += strideBL;
                            dstU1 += widthEL;
                            dstV1 += widthEL;
                        }
        }
        
    }
    
}







void ff_upsample_base_layer_frame_sse_v(struct AVFrame *FrameEL, struct AVFrame *FrameBL, short *Buffer[3], const int32_t enabled_up_sample_filter_luma[16][8], const int32_t enabled_up_sample_filter_chroma[16][4], struct HEVCWindow *Enhscal, struct UpsamplInf *up_info, int channel)
{
    
    int widthBL =  FrameBL->coded_width;
    int heightBL = FrameBL->coded_height;
    int strideBL = FrameBL->linesize[0];
    
    int widthEL =  FrameEL->width - Enhscal->left_offset - Enhscal->right_offset;
    int heightEL = FrameEL->height - Enhscal->top_offset - Enhscal->bottom_offset;
    int strideEL = FrameEL->linesize[0];
    
    
    
    int refPos16 = 0;
    int phase    = 0;
    int refPos   = 0;
    __m128i r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,r13,r14,r15;
    
    widthEL   = FrameEL->width;  //pcUsPic->getWidth ();
    heightEL  = FrameEL->height; //pcUsPic->getHeight();
    
    widthBL   = FrameBL->coded_width;
    heightBL  = FrameBL->coded_height <= heightEL ? FrameBL->coded_height:heightEL;  // min( FrameBL->height, heightEL);
    int CTB = 64;
    int leftStartL = Enhscal->left_offset;
    
    int topStartL  = Enhscal->top_offset;
    int bottomEndL = FrameEL->height - Enhscal->bottom_offset;
    int start = channel * CTB;
    int end = widthEL <= (channel+1) * CTB ? widthEL:(channel+1) * CTB;
    int rightEndL  = widthEL - Enhscal->right_offset;
    const int nShift = US_FILTER_PREC*2;
    int iOffset = 1 << (nShift - 1);
    r15= _mm_set1_epi32(iOffset);

    if(1) {
        const int32_t* coeff;
        
        
        int i,j, k;
        
        short buffer1[8];
        
        uint8_t *dstBufY = FrameEL->data[0];
        short *tempBufY = Buffer[0];
        
        uint8_t *dstY;
        
        short *srcY1;

        
        
        for( j = 0; j < heightEL; j++ )	{
            int y = av_clip(j, topStartL, bottomEndL-1);
            refPos16 = ((( y - topStartL )*up_info->scaleYLum + up_info->addYLum) >> 12);
            phase    = refPos16 & 15;
            refPos   = refPos16 >> 4;
            coeff = enabled_up_sample_filter_luma[phase];
            refPos -= ((NTAPS_LUMA>>1) - 1);
            srcY1 = tempBufY + refPos *widthEL+start;
            dstY = dstBufY + j * strideEL+start;
            if(refPos < 0)
                for( i = start; i < end; i++ )	{
                    
                    for(k= 0; k<-refPos ; k++)
                        buffer1[k] = srcY1[-refPos*widthEL]; //srcY1[(-refPos+k)*strideEL];
                    for(k= 0; k<8+refPos ; k++)
                        buffer1[-refPos+k] = srcY1[(-refPos+k)*widthEL];
                    *dstY = av_clip_pixel( (LumVer_FILTER(buffer1, coeff) + iOffset) >> (nShift));
                    
                    if( (i >= leftStartL) && (i <= rightEndL-2) )
                        srcY1++;
                    dstY++;
                }else if(refPos+8 > heightBL )
                    for( i = start; i < end; i++ )	{
                        
                        for(k= 0; k<heightBL-refPos ; k++)
                            buffer1[k] = srcY1[k*widthEL];
                        for(k= 0; k<8-(heightBL-refPos) ; k++)
                            buffer1[heightBL-refPos+k] = srcY1[(heightBL-refPos-1)*widthEL];
                        *dstY = av_clip_pixel( (LumVer_FILTER(buffer1, coeff) + iOffset) >> (nShift));
                        
                        if( (i >= leftStartL) && (i <= rightEndL-2) )
                            srcY1++;
                        dstY++;
                    }else{
                        for( i = start; i < leftStartL ; i++ )	{
                            
                            *dstY = av_clip_pixel( (LumVer_FILTER1(srcY1, coeff, widthEL) + iOffset) >> (nShift));
                            
                            dstY++;
                        }
                        
                        r8= _mm_loadu_si128((__m128i*)coeff); //32 bit data, need 2 full loads to get all 8
                        r9= _mm_loadu_si128((__m128i*)(coeff+4));
                        int limit;
                        if(end == widthEL)
                            limit = rightEndL-20;
                        else
                            limit = end;
                        for( ; i <= limit; i+=8 )	{
                            r0= _mm_loadu_si128((__m128i*)srcY1);
                            r1= _mm_loadu_si128((__m128i*)(srcY1+widthEL));
                            r2= _mm_loadu_si128((__m128i*)(srcY1+widthEL*2));
                            r3= _mm_loadu_si128((__m128i*)(srcY1+widthEL*3));
                            r4= _mm_loadu_si128((__m128i*)(srcY1+widthEL*4));
                            r5= _mm_loadu_si128((__m128i*)(srcY1+widthEL*5));
                            r6= _mm_loadu_si128((__m128i*)(srcY1+widthEL*6));
                            r7= _mm_loadu_si128((__m128i*)(srcY1+widthEL*7));
                            
                            //  	printf("coeff value : %d vs %d\n", _mm_extract_epi32(r8,0), coeff[0]);
                            
                            r11= _mm_shuffle_epi32(r8,_MM_SHUFFLE(0,0,0,0));
                            r11= _mm_packs_epi32(r11,r11);
                            r14= _mm_mullo_epi16(r11,r0);
                            r10= _mm_mulhi_epi16(r11,r0);
                            r12= _mm_unpacklo_epi16(r14,r10);
                            r13= _mm_unpackhi_epi16(r14,r10);
                            //   	printf("0 : %d vs %d\n",_mm_extract_epi32(r12,0),srcY1[0]*coeff[0]);
                            
                            r11= _mm_shuffle_epi32(r8,_MM_SHUFFLE(1,1,1,1));
                            r11= _mm_packs_epi32(r11,r11);
                            r0= _mm_mullo_epi16(r11,r1);
                            r10= _mm_mulhi_epi16(r11,r1);
                            r12= _mm_add_epi32(r12,_mm_unpacklo_epi16(r0,r10));
                            r13= _mm_add_epi32(r13,_mm_unpackhi_epi16(r0,r10));
                            //   	printf("1 : %d vs %d\n",_mm_extract_epi32(r12,0),srcY1[0]*coeff[0] + srcY1[widthEL]*coeff[1]);
                            
                            
                            r11= _mm_shuffle_epi32(r8,_MM_SHUFFLE(2,2,2,2));
                            r11= _mm_packs_epi32(r11,r11);
                            r0= _mm_mullo_epi16(r11,r2);
                            r10= _mm_mulhi_epi16(r11,r2);
                            r12= _mm_add_epi32(r12,_mm_unpacklo_epi16(r0,r10));
                            r13= _mm_add_epi32(r13,_mm_unpackhi_epi16(r0,r10));
                            //     	printf("2 : %d vs %d\n",_mm_extract_epi32(r13,0),srcY1[4]*coeff[0] + srcY1[widthEL+4]*coeff[1] + srcY1[widthEL*2+4]*coeff[2]);
                            
                            
                            r11= _mm_shuffle_epi32(r8,_MM_SHUFFLE(3,3,3,3));
                            r11= _mm_packs_epi32(r11,r11);
                            //printf("coeff value : %d vs %d\n", _mm_extract_epi16(r11,0), coeff[3]);
                            r0= _mm_mullo_epi16(r11,r3);
                            r10= _mm_mulhi_epi16(r11,r3);
                            r12= _mm_add_epi32(r12,_mm_unpacklo_epi16(r0,r10));
                            r13= _mm_add_epi32(r13,_mm_unpackhi_epi16(r0,r10));
                            
                            r11= _mm_shuffle_epi32(r9,_MM_SHUFFLE(0,0,0,0));
                            r11= _mm_packs_epi32(r11,r11);
                            r0= _mm_mullo_epi16(r11,r4);
                            r10= _mm_mulhi_epi16(r11,r4);
                            r12= _mm_add_epi32(r12,_mm_unpacklo_epi16(r0,r10));
                            r13= _mm_add_epi32(r13,_mm_unpackhi_epi16(r0,r10));
                            
                            r11= _mm_shuffle_epi32(r9,_MM_SHUFFLE(1,1,1,1));
                            r11= _mm_packs_epi32(r11,r11);
                            r0= _mm_mullo_epi16(r11,r5);
                            r10= _mm_mulhi_epi16(r11,r5);
                            r12= _mm_add_epi32(r12,_mm_unpacklo_epi16(r0,r10));
                            r13= _mm_add_epi32(r13,_mm_unpackhi_epi16(r0,r10));
                            
                            
                            r11= _mm_shuffle_epi32(r9,_MM_SHUFFLE(2,2,2,2));
                            r11= _mm_packs_epi32(r11,r11);
                            r0= _mm_mullo_epi16(r11,r6);
                            r10= _mm_mulhi_epi16(r11,r6);
                            r12= _mm_add_epi32(r12,_mm_unpacklo_epi16(r0,r10));
                            r13= _mm_add_epi32(r13,_mm_unpackhi_epi16(r0,r10));
                            
                            
                            r11= _mm_shuffle_epi32(r9,_MM_SHUFFLE(3,3,3,3));
                            r11= _mm_packs_epi32(r11,r11);
                            r0= _mm_mullo_epi16(r11,r7);
                            r10= _mm_mulhi_epi16(r11,r7);
                            r12= _mm_add_epi32(r12,_mm_unpacklo_epi16(r0,r10));
                            r13= _mm_add_epi32(r13,_mm_unpackhi_epi16(r0,r10));
                            
                            r0= _mm_add_epi32(r12,r15);
                            r10= _mm_add_epi32(r13,r15);
                            
                            r0= _mm_srai_epi32(r0,nShift);
                            r10= _mm_srai_epi32(r10,nShift);
                            
                            r0= _mm_packus_epi32(r0,r10);
                            r0= _mm_packus_epi16(r0,_mm_setzero_si128());
                            
                            _mm_storel_epi64((__m128i*)dstY,r0);
                            
                            
                            srcY1+=8;
                            dstY+=8;
                        }
                        if(end == widthEL)
                            limit = rightEndL-2;
                        else
                            limit = end;
                       // printf("i %d %d \n", i, limit);
                        for( ; i <= (limit); i++ )	{
                            
                            *dstY = av_clip_pixel( (LumVer_FILTER1(srcY1, coeff, widthEL) + iOffset) >> (nShift));
                            
                            
                            srcY1++;
                            dstY++;
                        }
                        for( ; i < end; i++ )	{
                            
                            *dstY = av_clip_pixel( (LumVer_FILTER1(srcY1, coeff, widthEL) + iOffset) >> (nShift));
                            
                            
                            dstY++;
                        }
                    }
        }
        
    }
    if( 1 ) {
        
        const int32_t* coeff;
        int i,j, k;
        
        short buffer1[8];
        
        uint8_t *dstBufU = FrameEL->data[1];
        short *tempBufU = Buffer[1];
        
        uint8_t *dstU;
        
        short *srcU1;
        
        
        uint8_t *dstBufV = FrameEL->data[2];
        short *tempBufV = Buffer[2];
        
        uint8_t *dstV;
        
        short *srcV1;
        
        
        widthBL   = FrameBL->coded_width;
        heightBL  = FrameBL->coded_height;
        
        widthEL   = FrameEL->width - Enhscal->right_offset - Enhscal->left_offset;
        heightEL  = FrameEL->height - Enhscal->top_offset - Enhscal->bottom_offset;
        
        widthEL  >>= 1;
        heightEL >>= 1;
        widthBL  >>= 1;
        heightBL >>= 1;
        strideBL  = FrameBL->linesize[1];
        strideEL  = FrameEL->linesize[1];
        int leftStartC = Enhscal->left_offset>>1;
        int rightEndC  = (FrameEL->width>>1) - (Enhscal->right_offset>>1);
        int topStartC  = Enhscal->top_offset>>1;
        int bottomEndC = (FrameEL->height>>1) - (Enhscal->bottom_offset>>1);
        
        
        widthEL   = FrameEL->width >> 1;
        heightEL  = FrameEL->height >> 1;
        widthBL   = FrameBL->coded_width >> 1;
        heightBL  = FrameBL->coded_height > heightEL ? FrameBL->coded_height:heightEL;
        
        
        heightBL >>= 1;
        
        start = channel * CTB/2;
        end = widthEL <= (channel+1) * CTB/2 ? widthEL:(channel+1) * CTB/2;
        
        
        
        for( j = 0; j < heightEL; j++ )	{
            int y = av_clip_c(j, topStartC, bottomEndC - 1);
            refPos16 = (((y - topStartC)*up_info->scaleYCr + up_info->addYCr) >> 12) - 4;
            phase    = refPos16 & 15;
            refPos   = refPos16 >> 4;
            coeff = enabled_up_sample_filter_chroma[phase];
            //if(i == 28)
         //   printf("j %d refPos16 %d phase %d refPos %d refPos %d \n", j, refPos16, phase, refPos, refPos - ((NTAPS_CHROMA>>1) - 1) );
            refPos -= ((NTAPS_CHROMA>>1) - 1);
            srcU1 = tempBufU  + refPos *widthEL+start;
            srcV1 = tempBufV  + refPos *widthEL+start;
            dstU = dstBufU + j*strideEL+start;
            dstV = dstBufV + j*strideEL+start;
            r8= _mm_loadu_si128((__m128i*)coeff); //32 bit data, need 2 full loads to get all 8
            
            if(refPos < 0)
                for( i = start; i < end; i++ )	{
                    
                    for(k= 0; k<-refPos ; k++){
                        buffer1[k] = srcU1[(-refPos)*widthEL];
                        buffer1[k+4] = srcV1[(-refPos)*widthEL];
                    }
                    for(k= 0; k<4+refPos ; k++){
                        buffer1[-refPos+k] = srcU1[(-refPos+k)*widthEL];
                        buffer1[-refPos+k+4] = srcV1[(-refPos+k)*widthEL];
                    }
                    *dstU = av_clip_pixel( (CroVer_FILTER(buffer1, coeff) + iOffset) >> (nShift));
                    *dstV = av_clip_pixel( (CroVer_FILTER((buffer1+4), coeff) + iOffset) >> (nShift));
                    
                    if( (i >= leftStartC) && (i <= rightEndC-2) )	{
                        srcU1++;
                        srcV1++;
                    }
                    dstU++;
                    dstV++;
                }else if(refPos+4 > heightBL )
                    for( i = start; i < end; i++ )	{
                        
                        for(k= 0; k< heightBL-refPos ; k++) {
                            buffer1[k] = srcU1[k*widthEL];
                            buffer1[k+4] = srcV1[k*widthEL];
                        }
                        for(k= 0; k<4-(heightBL-refPos) ; k++) {
                            buffer1[heightBL-refPos+k] = srcU1[(heightBL-refPos-1)*widthEL];
                            buffer1[heightBL-refPos+k+4] = srcV1[(heightBL-refPos-1)*widthEL];
                        }
                        *dstU = av_clip_pixel( (CroVer_FILTER(buffer1, coeff) + iOffset) >> (nShift));
                        
                        
                        *dstV = av_clip_pixel( (CroVer_FILTER((buffer1+4), coeff) + iOffset) >> (nShift));
                        
                        if( (i >= leftStartC) && (i <= rightEndC-2) )	{
                            srcU1++;
                            srcV1++;
                        }
                        dstU++;
                        dstV++;
                    }else{
                        
                        for(i = start ; i < leftStartC; i++ )	{
                            
                            
                            *dstU = av_clip_pixel( (CroVer_FILTER1(srcU1, coeff, widthEL) + iOffset) >> (nShift));
                            
                            
                            *dstV = av_clip_pixel( (CroVer_FILTER1(srcV1, coeff, widthEL) + iOffset) >> (nShift));
                            
                            dstU++;
                            dstV++;
                        }
                        int limit;
                        if(end == widthEL)
                            limit = rightEndC-2-7;
                        else
                            limit = end;
                        for( ; i <= limit; i+=8 )	{
                            
                            
                            r0= _mm_loadu_si128((__m128i*)srcU1);
                            r1= _mm_loadu_si128((__m128i*)(srcU1+widthEL));
                            r2= _mm_loadu_si128((__m128i*)(srcU1+widthEL*2));
                            r3= _mm_loadu_si128((__m128i*)(srcU1+widthEL*3));
                            
                            
                            
                            r11= _mm_shuffle_epi32(r8,_MM_SHUFFLE(0,0,0,0));
                            r11= _mm_packs_epi32(r11,r11);
                            r14= _mm_mullo_epi16(r11,r0);
                            r10= _mm_mulhi_epi16(r11,r0);
                            r12= _mm_unpacklo_epi16(r14,r10);
                            r13= _mm_unpackhi_epi16(r14,r10);
                            
                            r11= _mm_shuffle_epi32(r8,_MM_SHUFFLE(1,1,1,1));
                            r11= _mm_packs_epi32(r11,r11);
                            r0= _mm_mullo_epi16(r11,r1);
                            r10= _mm_mulhi_epi16(r11,r1);
                            r12= _mm_add_epi32(r12,_mm_unpacklo_epi16(r0,r10));
                            r13= _mm_add_epi32(r13,_mm_unpackhi_epi16(r0,r10));
                            
                            
                            r11= _mm_shuffle_epi32(r8,_MM_SHUFFLE(2,2,2,2));
                            r11= _mm_packs_epi32(r11,r11);
                            r0= _mm_mullo_epi16(r11,r2);
                            r10= _mm_mulhi_epi16(r11,r2);
                            r12= _mm_add_epi32(r12,_mm_unpacklo_epi16(r0,r10));
                            r13= _mm_add_epi32(r13,_mm_unpackhi_epi16(r0,r10));
                            
                            
                            r11= _mm_shuffle_epi32(r8,_MM_SHUFFLE(3,3,3,3));
                            r11= _mm_packs_epi32(r11,r11);
                            r0= _mm_mullo_epi16(r11,r3);
                            r10= _mm_mulhi_epi16(r11,r3);
                            r12= _mm_add_epi32(r12,_mm_unpacklo_epi16(r0,r10));
                            r13= _mm_add_epi32(r13,_mm_unpackhi_epi16(r0,r10));
                            
                            r0= _mm_add_epi32(r12,r15);
                            r10= _mm_add_epi32(r13,r15);
                            
                            r0= _mm_srai_epi32(r0,nShift);
                            r10= _mm_srai_epi32(r10,nShift);
                            
                            r0= _mm_packus_epi32(r0,r10);
                            r0= _mm_packus_epi16(r0,_mm_setzero_si128());
                            
                            
                            _mm_storel_epi64((__m128i*)dstU,r0);
                            
                            r0= _mm_loadu_si128((__m128i*)srcV1);
                            r1= _mm_loadu_si128((__m128i*)(srcV1+widthEL));
                            r2= _mm_loadu_si128((__m128i*)(srcV1+widthEL*2));
                            r3= _mm_loadu_si128((__m128i*)(srcV1+widthEL*3));
                            
                            
                            r11= _mm_shuffle_epi32(r8,_MM_SHUFFLE(0,0,0,0));
                            r11= _mm_packs_epi32(r11,r11);
                            r14= _mm_mullo_epi16(r11,r0);
                            r10= _mm_mulhi_epi16(r11,r0);
                            r12= _mm_unpacklo_epi16(r14,r10);
                            r13= _mm_unpackhi_epi16(r14,r10);
                            
                            r11= _mm_shuffle_epi32(r8,_MM_SHUFFLE(1,1,1,1));
                            r11= _mm_packs_epi32(r11,r11);
                            r0= _mm_mullo_epi16(r11,r1);
                            r10= _mm_mulhi_epi16(r11,r1);
                            r12= _mm_add_epi32(r12,_mm_unpacklo_epi16(r0,r10));
                            r13= _mm_add_epi32(r13,_mm_unpackhi_epi16(r0,r10));
                            
                            
                            r11= _mm_shuffle_epi32(r8,_MM_SHUFFLE(2,2,2,2));
                            r11= _mm_packs_epi32(r11,r11);
                            r0= _mm_mullo_epi16(r11,r2);
                            r10= _mm_mulhi_epi16(r11,r2);
                            r12= _mm_add_epi32(r12,_mm_unpacklo_epi16(r0,r10));
                            r13= _mm_add_epi32(r13,_mm_unpackhi_epi16(r0,r10));
                            
                            
                            r11= _mm_shuffle_epi32(r8,_MM_SHUFFLE(3,3,3,3));
                            r11= _mm_packs_epi32(r11,r11);
                            r0= _mm_mullo_epi16(r11,r3);
                            r10= _mm_mulhi_epi16(r11,r3);
                            r12= _mm_add_epi32(r12,_mm_unpacklo_epi16(r0,r10));
                            r13= _mm_add_epi32(r13,_mm_unpackhi_epi16(r0,r10));
                            
                            
                            r0= _mm_add_epi32(r12,r15);
                            r10= _mm_add_epi32(r13,r15);
                            
                            r0= _mm_srai_epi32(r0,nShift);
                            r10= _mm_srai_epi32(r10,nShift);
                            
                            r0= _mm_packus_epi32(r0,r10);
                            r0= _mm_packus_epi16(r0,_mm_setzero_si128());
                            
                            _mm_storel_epi64((__m128i*)dstV,r0);
                            
                            srcU1+=8;
                            srcV1+=8;
                            
                            dstU+=8;
                            dstV+=8;
                        }
                        if(end == widthEL)
                            limit = rightEndC-2;
                        else
                            limit = end;

                        for( ; i <= limit; i++ )	{
                            
                            *dstU = av_clip_pixel( (CroVer_FILTER1(srcU1, coeff, widthEL) + iOffset) >> (nShift));
                            
                            
                            *dstV = av_clip_pixel( (CroVer_FILTER1(srcV1, coeff, widthEL) + iOffset) >> (nShift));
                            
                            if( (i >= leftStartC) && (i <= limit) )	{
                                srcU1++;
                                srcV1++;
                            }
                            dstU++;
                            dstV++;
                        }
                        for( ; i < end; i++ )	{
                            
                            *dstU = av_clip_pixel( (CroVer_FILTER1(srcU1, coeff, widthEL) + iOffset) >> (nShift));
                            
                            
                            *dstV = av_clip_pixel( (CroVer_FILTER1(srcV1, coeff, widthEL) + iOffset) >> (nShift));
                            
                            
                            if( (i >= leftStartC) && (i <= rightEndC-2) )	{
                                srcU1++;
                                srcV1++;
                            }
                            dstU++;
                            dstV++;
                        }
                        
                        
                    }
        }
    }
    
}


void ff_upsample_base_layer_frame_sse(struct AVFrame *FrameEL, struct AVFrame *FrameBL, short *Buffer[3], const int16_t enabled_up_sample_filter_luma[16][8], const int16_t enabled_up_sample_filter_chroma[16][4], struct HEVCWindow *Enhscal, struct UpsamplInf *up_info, int channel)
{
    
    //ff_upsample_base_layer_frame_sse_h(FrameEL,FrameBL, Buffer, enabled_up_sample_filter_luma,enabled_up_sample_filter_chroma, Enhscal, up_info,  channel);
    //ff_upsample_base_layer_frame_sse_v(FrameEL,FrameBL, Buffer, enabled_up_sample_filter_luma,enabled_up_sample_filter_chroma, Enhscal, up_info,  channel);
    

    int widthBL =  FrameBL->coded_width;
    int heightBL = FrameBL->coded_height;
    int strideBL = FrameBL->linesize[0];
    
    int widthEL =  FrameEL->width - Enhscal->left_offset - Enhscal->right_offset;
    int heightEL = FrameEL->height - Enhscal->top_offset - Enhscal->bottom_offset;
    int strideEL = FrameEL->linesize[0];
    
    
        
    int refPos16 = 0;
    int phase    = 0;
    int refPos   = 0;
    __m128i r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,r13,r14,r15;
    
    widthEL   = FrameEL->width;  //pcUsPic->getWidth ();
    heightEL  = FrameEL->height; //pcUsPic->getHeight();
    
    widthBL   = FrameBL->coded_width;
    heightBL  = FrameBL->coded_height <= heightEL ? FrameBL->coded_height:heightEL;  // min( FrameBL->height, heightEL);
    
    int leftStartL = Enhscal->left_offset;
    int rightEndL  = FrameEL->width - Enhscal->right_offset;
    int topStartL  = Enhscal->top_offset;
    int bottomEndL = FrameEL->height - Enhscal->bottom_offset;
    
    const int nShift = US_FILTER_PREC*2;
    int iOffset = 1 << (nShift - 1);
   // printf("nShift %d iOffset %d \n", nShift, iOffset);
    r15= _mm_set1_epi32(iOffset);
    if(1 ) {
        const int16_t* coeff;
      
    
        int i,j, k;
        uint8_t buffer[8];
        short buffer1[8];
        uint8_t *srcBufY = FrameBL->data[0];
        uint8_t *dstBufY = FrameEL->data[0];
        short *tempBufY = Buffer[0];
        uint8_t *srcY;
        uint8_t *dstY;
        short *dstY1;
        short *srcY1;
        for( i = 0; i < widthEL; i++ )	{
            int x = av_clip_c(i, leftStartL, rightEndL);
            refPos16 = (((x - leftStartL)*up_info->scaleXLum + up_info->addXLum) >> 12);
            phase    = refPos16 & 15;
            refPos   = refPos16 >> 4;
            coeff = enabled_up_sample_filter_luma[phase];
            refPos -= ((NTAPS_LUMA>>1) - 1);
            srcY = srcBufY + refPos;
            dstY1 = tempBufY + i;
            if(refPos < 0)
                for( j = 0; j < heightBL ; j++ )	{
                    
                    memset(buffer, srcY[-refPos], -refPos);
                    memcpy(buffer-refPos, srcY-refPos, 8+refPos);
                    *dstY1 = LumHor_FILTER(buffer, coeff);
                    
                    srcY += strideBL;
                    dstY1 += widthEL;//strideEL;
                } else if(refPos+8 > widthBL )
                    for( j = 0; j < heightBL ; j++ )	{
                        
                        memcpy(buffer, srcY, widthBL-refPos);
                        memset(buffer+(widthBL-refPos), srcY[widthBL-refPos-1], 8-(widthBL-refPos));
                        *dstY1 = LumHor_FILTER(buffer, coeff);
                        srcY += strideBL;
                        dstY1 += widthEL;//strideEL;
                    }else
                        for( j = 0; j < heightBL ; j++ )	{
                            
                            *dstY1 = LumHor_FILTER(srcY, coeff);
                          
                            srcY += strideBL;
                            dstY1 += widthEL;//strideEL;
                        }
            
        }
        /*
         
         */
        
        
        
        
        
        
        
        for( j = 0; j < heightEL; j++ )	{
            int y = av_clip_c(j, topStartL, bottomEndL-1);
            refPos16 = ((( y - topStartL )*up_info->scaleYLum + up_info->addYLum) >> 12);
            phase    = refPos16 & 15;
            refPos   = refPos16 >> 4;
            coeff = enabled_up_sample_filter_luma[phase];
         //   printf("%d  ", phase);
            refPos -= ((NTAPS_LUMA>>1) - 1);
            srcY1 = tempBufY + refPos *widthEL;
            dstY = dstBufY + j * strideEL;
            if(refPos < 0)
                for( i = 0; i < widthEL; i++ )	{
                    
                    for(k= 0; k<-refPos ; k++)
                        buffer1[k] = srcY1[-refPos*widthEL]; //srcY1[(-refPos+k)*strideEL];
                    for(k= 0; k<8+refPos ; k++)
                        buffer1[-refPos+k] = srcY1[(-refPos+k)*widthEL];
                    *dstY = av_clip_pixel( (LumVer_FILTER(buffer1, coeff) + iOffset) >> (nShift));
                    
                    if( (i >= leftStartL) && (i <= rightEndL-2) )
                        srcY1++;
                    dstY++;
                }else if(refPos+8 > heightBL )
                    for( i = 0; i < widthEL; i++ )	{
                        
                        for(k= 0; k<heightBL-refPos ; k++)
                            buffer1[k] = srcY1[k*widthEL];
                        for(k= 0; k<8-(heightBL-refPos) ; k++)
                            buffer1[heightBL-refPos+k] = srcY1[(heightBL-refPos-1)*widthEL];
                        *dstY = av_clip_pixel( (LumVer_FILTER(buffer1, coeff) + iOffset) >> (nShift));
                        
                        if( (i >= leftStartL) && (i <= rightEndL-2) )
                            srcY1++;
                        dstY++;
                    }else{
                        for( i = 0; i < leftStartL; i++ )	{
                            
                            *dstY = av_clip_pixel( (LumVer_FILTER1(srcY1, coeff, widthEL) + iOffset) >> (nShift));
                            
                            dstY++;
                        }
                        
                        r8= _mm_loadu_si128((__m128i*)coeff); //32 bit data, need 2 full loads to get all 8
                        r9= _mm_loadu_si128((__m128i*)(coeff+4));
                        for( ; i <= (rightEndL-20); i+=8 )	{
                            r0= _mm_loadu_si128((__m128i*)srcY1);
                            r1= _mm_loadu_si128((__m128i*)(srcY1+widthEL));
                            r2= _mm_loadu_si128((__m128i*)(srcY1+widthEL*2));
                            r3= _mm_loadu_si128((__m128i*)(srcY1+widthEL*3));
                            r4= _mm_loadu_si128((__m128i*)(srcY1+widthEL*4));
                            r5= _mm_loadu_si128((__m128i*)(srcY1+widthEL*5));
                            r6= _mm_loadu_si128((__m128i*)(srcY1+widthEL*6));
                            r7= _mm_loadu_si128((__m128i*)(srcY1+widthEL*7));
                            
                            //  	printf("coeff value : %d vs %d\n", _mm_extract_epi32(r8,0), coeff[0]);
                            
                            r11= _mm_shuffle_epi32(r8,_MM_SHUFFLE(0,0,0,0));
                            r11= _mm_packs_epi32(r11,r11);
                            r14= _mm_mullo_epi16(r11,r0);
                            r10= _mm_mulhi_epi16(r11,r0);
                            r12= _mm_unpacklo_epi16(r14,r10);
                            r13= _mm_unpackhi_epi16(r14,r10);
                            //   	printf("0 : %d vs %d\n",_mm_extract_epi32(r12,0),srcY1[0]*coeff[0]);
                            
                            r11= _mm_shuffle_epi32(r8,_MM_SHUFFLE(1,1,1,1));
                            r11= _mm_packs_epi32(r11,r11);
                            r0= _mm_mullo_epi16(r11,r1);
                            r10= _mm_mulhi_epi16(r11,r1);
                            r12= _mm_add_epi32(r12,_mm_unpacklo_epi16(r0,r10));
                            r13= _mm_add_epi32(r13,_mm_unpackhi_epi16(r0,r10));
                            //   	printf("1 : %d vs %d\n",_mm_extract_epi32(r12,0),srcY1[0]*coeff[0] + srcY1[widthEL]*coeff[1]);
                            
                            
                            r11= _mm_shuffle_epi32(r8,_MM_SHUFFLE(2,2,2,2));
                            r11= _mm_packs_epi32(r11,r11);
                            r0= _mm_mullo_epi16(r11,r2);
                            r10= _mm_mulhi_epi16(r11,r2);
                            r12= _mm_add_epi32(r12,_mm_unpacklo_epi16(r0,r10));
                            r13= _mm_add_epi32(r13,_mm_unpackhi_epi16(r0,r10));
                            //     	printf("2 : %d vs %d\n",_mm_extract_epi32(r13,0),srcY1[4]*coeff[0] + srcY1[widthEL+4]*coeff[1] + srcY1[widthEL*2+4]*coeff[2]);
                            
                            
                            r11= _mm_shuffle_epi32(r8,_MM_SHUFFLE(3,3,3,3));
                            r11= _mm_packs_epi32(r11,r11);
                            //printf("coeff value : %d vs %d\n", _mm_extract_epi16(r11,0), coeff[3]);
                            r0= _mm_mullo_epi16(r11,r3);
                            r10= _mm_mulhi_epi16(r11,r3);
                            r12= _mm_add_epi32(r12,_mm_unpacklo_epi16(r0,r10));
                            r13= _mm_add_epi32(r13,_mm_unpackhi_epi16(r0,r10));
                            // 	printf("3 : %d vs %d\n",_mm_extract_epi32(r12,0),srcY1[0]*coeff[0] + srcY1[widthEL]*coeff[1] + srcY1[widthEL*2]*coeff[2]+ srcY1[widthEL*3]*coeff[3]);
                            
                            
                            r11= _mm_shuffle_epi32(r9,_MM_SHUFFLE(0,0,0,0));
                            r11= _mm_packs_epi32(r11,r11);
                            r0= _mm_mullo_epi16(r11,r4);
                            r10= _mm_mulhi_epi16(r11,r4);
                            r12= _mm_add_epi32(r12,_mm_unpacklo_epi16(r0,r10));
                            r13= _mm_add_epi32(r13,_mm_unpackhi_epi16(r0,r10));
                            //      	printf("4 : %d vs %d\n",_mm_extract_epi32(r13,0),srcY1[4]*coeff[0] + srcY1[widthEL+4]*coeff[1] + srcY1[widthEL*2+4]*coeff[2]+ srcY1[widthEL*3+4]*coeff[3]+ srcY1[widthEL*4+4]*coeff[4]);
                            
                            
                            r11= _mm_shuffle_epi32(r9,_MM_SHUFFLE(1,1,1,1));
                            r11= _mm_packs_epi32(r11,r11);
                            r0= _mm_mullo_epi16(r11,r5);
                            r10= _mm_mulhi_epi16(r11,r5);
                            r12= _mm_add_epi32(r12,_mm_unpacklo_epi16(r0,r10));
                            r13= _mm_add_epi32(r13,_mm_unpackhi_epi16(r0,r10));
                            //      	printf("5 : %d vs %d\n",_mm_extract_epi32(r13,0),srcY1[4]*coeff[0] + srcY1[widthEL+4]*coeff[1] + srcY1[widthEL*2+4]*coeff[2]+ srcY1[widthEL*3+4]*coeff[3]+ srcY1[widthEL*4+4]*coeff[4]+ srcY1[widthEL*5+4]*coeff[5]);
                            
                            
                            r11= _mm_shuffle_epi32(r9,_MM_SHUFFLE(2,2,2,2));
                            r11= _mm_packs_epi32(r11,r11);
                            r0= _mm_mullo_epi16(r11,r6);
                            r10= _mm_mulhi_epi16(r11,r6);
                            r12= _mm_add_epi32(r12,_mm_unpacklo_epi16(r0,r10));
                            r13= _mm_add_epi32(r13,_mm_unpackhi_epi16(r0,r10));
                            //	printf("6 : %d vs %d\n",_mm_extract_epi32(r13,0),srcY1[0]*coeff[0] + srcY1[widthEL]*coeff[1] + srcY1[widthEL*2]*coeff[2]+ srcY1[widthEL*3]*coeff[3]+ srcY1[widthEL*4]*coeff[4]+ srcY1[widthEL*5]*coeff[5]+ srcY1[widthEL*6]*coeff[6]);
                            
                            
                            r11= _mm_shuffle_epi32(r9,_MM_SHUFFLE(3,3,3,3));
                            r11= _mm_packs_epi32(r11,r11);
                            r0= _mm_mullo_epi16(r11,r7);
                            r10= _mm_mulhi_epi16(r11,r7);
                            r12= _mm_add_epi32(r12,_mm_unpacklo_epi16(r0,r10));
                            r13= _mm_add_epi32(r13,_mm_unpackhi_epi16(r0,r10));
                            //srcY1+=4;
                            //  	printf("coeff add : %d vs %d\n",_mm_extract_epi32(r12,0),LumVer_FILTER1(srcY1, coeff, widthEL));
                            //srcY1-=4;
                            
                            r0= _mm_add_epi32(r12,r15);
                            r10= _mm_add_epi32(r13,r15);
                            //  	printf("coeff ioff : %d vs %d\n",_mm_extract_epi32(r0,0),LumVer_FILTER1(srcY1, coeff, widthEL)+iOffset);
                            
                            r0= _mm_srai_epi32(r0,nShift);
                            r10= _mm_srai_epi32(r10,nShift);
                            //    	printf("coeff shift : %d vs %d\n",_mm_extract_epi32(r0,0),(LumVer_FILTER1(srcY1, coeff, widthEL)+iOffset)>>nShift);
                            
                            r0= _mm_packus_epi32(r0,r10);
                            r0= _mm_packus_epi16(r0,_mm_setzero_si128());
                            
                            _mm_storel_epi64((__m128i*)dstY,r0);
                            
                            //srcY1+=7;
                            //if(dstY[0] != av_clip_pixel( (LumVer_FILTER1(srcY1, coeff, widthEL) + iOffset) >> (nShift)))
                            //printf("dstY = %d vs %d\n",dstY[0],av_clip_pixel( (LumVer_FILTER1(srcY1, coeff, widthEL) + iOffset) >> (nShift)));
                            //srcY1-=7;
                            //*dstY = av_clip_pixel( (LumVer_FILTER1(srcY1, coeff, widthEL) + iOffset) >> (nShift));
                            
                            srcY1+=8;
                            dstY+=8;
                        }
                        
                        for( ; i <= (rightEndL-2); i++ )	{
                            
                            *dstY = av_clip_pixel( (LumVer_FILTER1(srcY1, coeff, widthEL) + iOffset) >> (nShift));
                            
                            
                            srcY1++;
                            dstY++;
                        }
                        for( ; i < widthEL; i++ )	{
                            
                            *dstY = av_clip_pixel( (LumVer_FILTER1(srcY1, coeff, widthEL) + iOffset) >> (nShift));
                            //printf("%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d \n", *dstY, srcY1[0], srcY1[1*widthEL], srcY1[2*widthEL], srcY1[3*widthEL], srcY1[4*widthEL], srcY1[5*widthEL], srcY1[6*widthEL], srcY1[7*widthEL], coeff[0], coeff[1], coeff[2], coeff[3], coeff[4], coeff[5], coeff[6], coeff[7]);
                            
                            dstY++;
                        }
                    }
        
        }
        /*uint8_t *data = FrameEL->data[0];;
        
        for(i=0; i < heightEL; i++){
            printf("%d ", i);
            for(j=0; j < widthEL; j++){
                printf("%d ", data[0]);
                data++;
            }
            printf("\n");
            data+= widthEL;
        }*/
       
    }
    if( 1 ) {
       
        const int16_t* coeff;
        int i,j, k;
        uint8_t buffer[8];
        short buffer1[8];
        uint8_t *srcBufU = FrameBL->data[1];
        uint8_t *dstBufU = FrameEL->data[1];
        short *tempBufU = Buffer[1];
        uint8_t *srcU;
        uint8_t *dstU;
        short *dstU1;
        short *srcU1;
        
        uint8_t *srcBufV = FrameBL->data[2];
        uint8_t *dstBufV = FrameEL->data[2];
        short *tempBufV = Buffer[2];
        uint8_t *srcV;
        uint8_t *dstV;
        short *dstV1;
        short *srcV1;
        
    
        widthBL   = FrameBL->coded_width;
        heightBL  = FrameBL->coded_height;
        
        widthEL   = FrameEL->width - Enhscal->right_offset - Enhscal->left_offset;
        heightEL  = FrameEL->height - Enhscal->top_offset - Enhscal->bottom_offset;
        
        widthEL  >>= 1;
        heightEL >>= 1;
        widthBL  >>= 1;
        heightBL >>= 1;
        strideBL  = FrameBL->linesize[1];
        strideEL  = FrameEL->linesize[1];
        int leftStartC = Enhscal->left_offset>>1;
        int rightEndC  = (FrameEL->width>>1) - (Enhscal->right_offset>>1);
        int topStartC  = Enhscal->top_offset>>1;
        int bottomEndC = (FrameEL->height>>1) - (Enhscal->bottom_offset>>1);
        
        
        widthEL   = FrameEL->width >> 1;
        heightEL  = FrameEL->height >> 1;
        widthBL   = FrameBL->coded_width >> 1;
        heightBL  = FrameBL->coded_height > heightEL ? FrameBL->coded_height:heightEL;
   
        
        heightBL >>= 1;
        
        //========== horizontal upsampling ===========
        for( i = 0; i < widthEL; i++ )	{
            int x = av_clip_c(i, leftStartC, rightEndC - 1);
            refPos16 = (((x - leftStartC)*up_info->scaleXCr + up_info->addXCr) >> 12);
            phase    = refPos16 & 15;
            refPos   = refPos16 >> 4;
            coeff = enabled_up_sample_filter_chroma[phase];
            
            refPos -= ((NTAPS_CHROMA>>1) - 1);
            srcU = srcBufU + refPos; // -((NTAPS_CHROMA>>1) - 1);
            srcV = srcBufV + refPos; // -((NTAPS_CHROMA>>1) - 1);
            dstU1 = tempBufU + i;
            dstV1 = tempBufV + i;
            
            if(refPos < 0)
                for( j = 0; j < heightBL ; j++ )	{
                    
                    memset(buffer, srcU[-refPos], -refPos);
                    memcpy(buffer-refPos, srcU-refPos, 4+refPos);
                    memset(buffer+4, srcV[-refPos], -refPos);
                    memcpy(buffer-refPos+4, srcV-refPos, 4+refPos);
                    
                    *dstU1 = CroHor_FILTER(buffer, coeff);
                    
                    *dstV1 = CroHor_FILTER((buffer+4), coeff);
                    
                    
                    srcU += strideBL;
                    srcV += strideBL;
                    dstU1 += widthEL;
                    dstV1 += widthEL;
                }else if(refPos+4 > widthBL )
                    for( j = 0; j < heightBL ; j++ )	{
                        
                        memcpy(buffer, srcU, widthBL-refPos);
                        memset(buffer+(widthBL-refPos), srcU[widthBL-refPos-1], 4-(widthBL-refPos));
                        
                        memcpy(buffer+4, srcV, widthBL-refPos);
                        memset(buffer+(widthBL-refPos)+4, srcV[widthBL-refPos-1], 4-(widthBL-refPos));
                        
                        *dstU1 = CroHor_FILTER(buffer, coeff);
                        
                        *dstV1 = CroHor_FILTER((buffer+4), coeff);
                        
                        srcU += strideBL;
                        srcV += strideBL;
                        dstU1 += widthEL;
                        dstV1 += widthEL;
                    }else
                        for( j = 0; j < heightBL ; j++ )	{
                            
                            *dstU1 = CroHor_FILTER(srcU, coeff);
                            
                            *dstV1 = CroHor_FILTER(srcV, coeff);
                            
                            
                            srcU += strideBL;
                            srcV += strideBL;
                            dstU1 += widthEL;
                            dstV1 += widthEL;
                        }
        }
        
        
        for( j = 0; j < heightEL; j++ )	{
            int y = av_clip_c(j, topStartC, bottomEndC - 1);
            refPos16 = (((y - topStartC)*up_info->scaleYCr + up_info->addYCr) >> 12) - 4;
            phase    = refPos16 & 15;
            refPos   = refPos16 >> 4;
            coeff = enabled_up_sample_filter_chroma[phase];
            refPos -= ((NTAPS_CHROMA>>1) - 1);
            srcU1 = tempBufU  + refPos *widthEL;
            srcV1 = tempBufV  + refPos *widthEL;
            dstU = dstBufU + j*strideEL;
            dstV = dstBufV + j*strideEL;
            r8= _mm_loadu_si128((__m128i*)coeff); //32 bit data, need 2 full loads to get all 8
            
            if(refPos < 0)
                for( i = 0; i < widthEL; i++ )	{
                    
                    for(k= 0; k<-refPos ; k++){
                        buffer1[k] = srcU1[(-refPos)*widthEL];
                        buffer1[k+4] = srcV1[(-refPos)*widthEL];
                    }
                    for(k= 0; k<4+refPos ; k++){
                        buffer1[-refPos+k] = srcU1[(-refPos+k)*widthEL];
                        buffer1[-refPos+k+4] = srcV1[(-refPos+k)*widthEL];
                    }
                    *dstU = av_clip_pixel( (CroVer_FILTER(buffer1, coeff) + iOffset) >> (nShift));
                    *dstV = av_clip_pixel( (CroVer_FILTER((buffer1+4), coeff) + iOffset) >> (nShift));
                    
                    if( (i >= leftStartC) && (i <= rightEndC-2) )	{
                        srcU1++;
                        srcV1++;
                    }
                    dstU++;
                    dstV++;
                }else if(refPos+4 > heightBL )
                    for( i = 0; i < widthEL; i++ )	{
                        
                        for(k= 0; k< heightBL-refPos ; k++) {
                            buffer1[k] = srcU1[k*widthEL];
                            buffer1[k+4] = srcV1[k*widthEL];
                        }
                        for(k= 0; k<4-(heightBL-refPos) ; k++) {
                            buffer1[heightBL-refPos+k] = srcU1[(heightBL-refPos-1)*widthEL];
                            buffer1[heightBL-refPos+k+4] = srcV1[(heightBL-refPos-1)*widthEL];
                        }
                        *dstU = av_clip_pixel( (CroVer_FILTER(buffer1, coeff) + iOffset) >> (nShift));
                        
                        
                        *dstV = av_clip_pixel( (CroVer_FILTER((buffer1+4), coeff) + iOffset) >> (nShift));
                        
                        if( (i >= leftStartC) && (i <= rightEndC-2) )	{
                            srcU1++;
                            srcV1++;
                        }
                        dstU++;
                        dstV++;
                    }else{
                        
                        for(i = 0 ; i < leftStartC; i++ )	{
                            
                            *dstU = av_clip_pixel( (CroVer_FILTER1(srcU1, coeff, widthEL) + iOffset) >> (nShift));
                            
                            
                            *dstV = av_clip_pixel( (CroVer_FILTER1(srcV1, coeff, widthEL) + iOffset) >> (nShift));
                            
                            dstU++;
                            dstV++;
                        }
                        
                        for( ; i <= rightEndC-2-7; i+=8 )	{
                            
                            
                            r0= _mm_loadu_si128((__m128i*)srcU1);
                            r1= _mm_loadu_si128((__m128i*)(srcU1+widthEL));
                            r2= _mm_loadu_si128((__m128i*)(srcU1+widthEL*2));
                            r3= _mm_loadu_si128((__m128i*)(srcU1+widthEL*3));
                            
                            
                            
                            r11= _mm_shuffle_epi32(r8,_MM_SHUFFLE(0,0,0,0));
                            r11= _mm_packs_epi32(r11,r11);
                            r14= _mm_mullo_epi16(r11,r0);
                            r10= _mm_mulhi_epi16(r11,r0);
                            r12= _mm_unpacklo_epi16(r14,r10);
                            r13= _mm_unpackhi_epi16(r14,r10);
                            
                            r11= _mm_shuffle_epi32(r8,_MM_SHUFFLE(1,1,1,1));
                            r11= _mm_packs_epi32(r11,r11);
                            r0= _mm_mullo_epi16(r11,r1);
                            r10= _mm_mulhi_epi16(r11,r1);
                            r12= _mm_add_epi32(r12,_mm_unpacklo_epi16(r0,r10));
                            r13= _mm_add_epi32(r13,_mm_unpackhi_epi16(r0,r10));
                            
                            
                            r11= _mm_shuffle_epi32(r8,_MM_SHUFFLE(2,2,2,2));
                            r11= _mm_packs_epi32(r11,r11);
                            r0= _mm_mullo_epi16(r11,r2);
                            r10= _mm_mulhi_epi16(r11,r2);
                            r12= _mm_add_epi32(r12,_mm_unpacklo_epi16(r0,r10));
                            r13= _mm_add_epi32(r13,_mm_unpackhi_epi16(r0,r10));
                            
                            
                            r11= _mm_shuffle_epi32(r8,_MM_SHUFFLE(3,3,3,3));
                            r11= _mm_packs_epi32(r11,r11);
                            r0= _mm_mullo_epi16(r11,r3);
                            r10= _mm_mulhi_epi16(r11,r3);
                            r12= _mm_add_epi32(r12,_mm_unpacklo_epi16(r0,r10));
                            r13= _mm_add_epi32(r13,_mm_unpackhi_epi16(r0,r10));
                            
                            r0= _mm_add_epi32(r12,r15);
                            r10= _mm_add_epi32(r13,r15);
                            
                            r0= _mm_srai_epi32(r0,nShift);
                            r10= _mm_srai_epi32(r10,nShift);
                            
                            r0= _mm_packus_epi32(r0,r10);
                            r0= _mm_packus_epi16(r0,_mm_setzero_si128());
                            
                            
                            _mm_storel_epi64((__m128i*)dstU,r0);
                            
                            r0= _mm_loadu_si128((__m128i*)srcV1);
                            r1= _mm_loadu_si128((__m128i*)(srcV1+widthEL));
                            r2= _mm_loadu_si128((__m128i*)(srcV1+widthEL*2));
                            r3= _mm_loadu_si128((__m128i*)(srcV1+widthEL*3));
                            
                            
                            r11= _mm_shuffle_epi32(r8,_MM_SHUFFLE(0,0,0,0));
                            r11= _mm_packs_epi32(r11,r11);
                            r14= _mm_mullo_epi16(r11,r0);
                            r10= _mm_mulhi_epi16(r11,r0);
                            r12= _mm_unpacklo_epi16(r14,r10);
                            r13= _mm_unpackhi_epi16(r14,r10);
                            
                            r11= _mm_shuffle_epi32(r8,_MM_SHUFFLE(1,1,1,1));
                            r11= _mm_packs_epi32(r11,r11);
                            r0= _mm_mullo_epi16(r11,r1);
                            r10= _mm_mulhi_epi16(r11,r1);
                            r12= _mm_add_epi32(r12,_mm_unpacklo_epi16(r0,r10));
                            r13= _mm_add_epi32(r13,_mm_unpackhi_epi16(r0,r10));
                            
                            
                            r11= _mm_shuffle_epi32(r8,_MM_SHUFFLE(2,2,2,2));
                            r11= _mm_packs_epi32(r11,r11);
                            r0= _mm_mullo_epi16(r11,r2);
                            r10= _mm_mulhi_epi16(r11,r2);
                            r12= _mm_add_epi32(r12,_mm_unpacklo_epi16(r0,r10));
                            r13= _mm_add_epi32(r13,_mm_unpackhi_epi16(r0,r10));
                            
                            
                            r11= _mm_shuffle_epi32(r8,_MM_SHUFFLE(3,3,3,3));
                            r11= _mm_packs_epi32(r11,r11);
                            r0= _mm_mullo_epi16(r11,r3);
                            r10= _mm_mulhi_epi16(r11,r3);
                            r12= _mm_add_epi32(r12,_mm_unpacklo_epi16(r0,r10));
                            r13= _mm_add_epi32(r13,_mm_unpackhi_epi16(r0,r10));
                            
                            
                            r0= _mm_add_epi32(r12,r15);
                            r10= _mm_add_epi32(r13,r15);
                            
                            r0= _mm_srai_epi32(r0,nShift);
                            r10= _mm_srai_epi32(r10,nShift);
                            
                            r0= _mm_packus_epi32(r0,r10);
                            r0= _mm_packus_epi16(r0,_mm_setzero_si128());
                            
                            _mm_storel_epi64((__m128i*)dstV,r0);
                            
                            srcU1+=8;
                            srcV1+=8;
                            
                            dstU+=8;
                            dstV+=8;
                        }
                        
                        for( ; i <= rightEndC-2; i++ )	{
                            
                            *dstU = av_clip_pixel( (CroVer_FILTER1(srcU1, coeff, widthEL) + iOffset) >> (nShift));
                            
                            
                            *dstV = av_clip_pixel( (CroVer_FILTER1(srcV1, coeff, widthEL) + iOffset) >> (nShift));
                            
                            if( (i >= leftStartC) && (i <= rightEndC-2) )	{
                                srcU1++;
                                srcV1++;
                            }
                            dstU++;
                            dstV++;
                        }
                        for( ; i < widthEL; i++ )	{
                            
                            *dstU = av_clip_pixel( (CroVer_FILTER1(srcU1, coeff, widthEL) + iOffset) >> (nShift));
                            
                            
                            *dstV = av_clip_pixel( (CroVer_FILTER1(srcV1, coeff, widthEL) + iOffset) >> (nShift));
                            
                            if( (i >= leftStartC) && (i <= rightEndC-2) )	{
                                srcU1++;
                                srcV1++;
                            }
                            dstU++;
                            dstV++;
                        }
                        
                        
                    }
            
        }
    }

}

#define UPSAMPLE_H_TABLE()                                     \
for( i = 0; i < block_w; i+=8 ){                                \
    m5      =   _mm_adds_epu16(m0,m3);                          \
    m5      =   _mm_min_epu16(m5,m4);                           \
    m7      =   _mm_mullo_epi16(m5,m1);                         \
    m5      =   _mm_mulhi_epu16(m5,m1);                         \
    m8      =   _mm_unpackhi_epi16(m7,m5);                      \
    m5      =   _mm_unpacklo_epi16(m7,m5);                      \
    m5      =   _mm_add_epi32(m5,m2);                           \
    m5      =   _mm_srli_epi32(m5,12);                          \
    m8      =   _mm_add_epi32(m8,m2);                           \
    m8      =   _mm_srli_epi32(m8,12);                          \
    _mm_storeu_si128((__m128i *) &temp[i],m5);                  \
    _mm_storeu_si128((__m128i *) &temp[i+4],m8);                \
    m0      =   _mm_add_epi16(m0,m6);                           \
}

ff_upsample_filter_block_luma_h_2_8_sse( int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                    int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                                    const int8_t enabled_up_sample_filter_luma[16][8], struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {


    int rightEndL  = widthEL - Enhscal->right_offset;
    int leftStartL = Enhscal->left_offset;
    int x, i, j, phase, refPos16, refPos, refPos16_2, refPos_2, refPos_3,phase_2;
    int16_t*   dst_tmp= dst;
    uint8_t*   src_tmp= _src, *src_tmp1;
    int8_t*   coeff;
    int * temp= NULL;
    __m128i     m0, m1, m2,m3,m4,m5, m6,m7,m8, r0,r1,r2,c0,c1,c2,c3;
    c0= _mm_setzero_si128();
    temp= av_malloc((block_w+16)*sizeof(int));
    m0= _mm_set_epi16(7,6,5,4,3,2,1,0);
    m1= _mm_set1_epi16(up_info->scaleXLum);
    m2= _mm_set1_epi32(up_info->addXLum);
    m3= _mm_set1_epi16(x_EL-leftStartL);
    m4= _mm_set1_epi16(rightEndL-leftStartL);
    m6= _mm_set1_epi16(8);

    UPSAMPLE_H_TABLE();

    for( j = 0; j < block_h ; j++ ) {
        src_tmp  = _src+_srcstride*j;
        dst_tmp  = dst + dststride*j;
        refPos= 0;
        for( i = 0; i < block_w-1; i+=2 ){
            refPos16 = temp[i];
            refPos16_2 = temp[i+1];
            phase    = refPos16 & 15;
            phase_2  = refPos16_2 & 15;
            coeff    = enabled_up_sample_filter_luma[phase];
            refPos   = (refPos16 >> 4) - x_BL;

            c1       = _mm_loadl_epi64((__m128i *)&enabled_up_sample_filter_luma[refPos16 & 15]);
            c2       = _mm_loadl_epi64((__m128i *)&enabled_up_sample_filter_luma[refPos16_2 & 15]);
            c2       = _mm_slli_si128(c2,8);
            c1       = _mm_or_si128(c1,c2);

            refPos_2   = (refPos16_2 >> 4) - x_BL;
            refPos_3   = refPos_2 - refPos;

            m0      =   _mm_loadu_si128((__m128i *) (src_tmp+ refPos - 3));
            if(refPos_3){
            m1      =   _mm_srli_si128(m0,1);
            }
            else{
                m1  =   _mm_srli_si128(m0,0);
            }
            m0      =   _mm_unpacklo_epi64(m0,m1);

            r0      =   _mm_maddubs_epi16(m0,c1);

            r0      =   _mm_hadd_epi16(r0,c0);
            r0      =   _mm_hadd_epi16(r0,c0);

            _mm_maskmoveu_si128(r0, _mm_set_epi32(0,0,0,-1),(char *)(dst_tmp+i));

        }

    }
    av_freep(&temp);
}
#define UPSAMPLE_L_LOAD_H()                                                                     \
m14       = _mm_loadl_epi64((__m128i *)&enabled_up_sample_filter_luma[refPos16 & 15]);      \
m13       = _mm_loadl_epi64((__m128i *)&enabled_up_sample_filter_luma[refPos16_2 & 15]);    \
m12       = _mm_loadl_epi64((__m128i *)&enabled_up_sample_filter_luma[refPos16_3 & 15]);    \
m11       = _mm_loadl_epi64((__m128i *)&enabled_up_sample_filter_luma[refPos16_4 & 15]);    \
m13       = _mm_slli_si128(m13,8);                                                          \
m14       = _mm_or_si128(m14,m13);                                                          \
m11       = _mm_slli_si128(m11,8);                                                          \
m12       = _mm_or_si128(m12,m11)

ff_upsample_filter_block_luma_h_8_8_sse( int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                    int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                                    const int8_t enabled_up_sample_filter_luma[16][8], struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {


    int rightEndL  = widthEL - Enhscal->right_offset;
    int leftStartL = Enhscal->left_offset;
    int x, i, j, phase, refPos16, refPos, refPos16_2, refPos_2, refPos_3,refPos16_4, refPos16_3, refPos16_5, refPos16_6, refPos16_7, refPos16_8;
    int16_t*   dst_tmp= dst;
    uint8_t*   src_tmp= _src, *src_tmp1;
    int8_t*   coeff;
    int * temp= NULL;
    __m128i     m0, m1, m2,m3,m4,m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15;
    m15= _mm_setzero_si128();
    temp= av_malloc((block_w+16)*sizeof(int));
    m0= _mm_set_epi16(7,6,5,4,3,2,1,0);
    m1= _mm_set1_epi16(up_info->scaleXLum);
    m2= _mm_set1_epi32(up_info->addXLum);
    m3= _mm_set1_epi16(x_EL-leftStartL);
    m4= _mm_set1_epi16(rightEndL-leftStartL);
    m6= _mm_set1_epi16(8);

    UPSAMPLE_H_TABLE();

    for( j = 0; j < block_h ; j++ ) {
        src_tmp  = _src+_srcstride*j;
        dst_tmp  = dst + dststride*j;
        refPos= 0;
        for( i = 0; i < block_w; i+=8){
            refPos16 = temp[i];
            refPos16_2 = temp[i+1];
            refPos16_3 = temp[i+2];
            refPos16_4 = temp[i+3];


            phase    = refPos16 & 15;
            coeff    = enabled_up_sample_filter_luma[phase];


            UPSAMPLE_L_LOAD_H();


            refPos   = (refPos16 >> 4) - x_BL;
            refPos_2   = (refPos16_2 >> 4) - x_BL;
            refPos_3   = refPos_2 - refPos;

            m0      =   _mm_loadu_si128((__m128i *) (src_tmp+ refPos - 3));
            if(refPos_3){
            m1      =   _mm_srli_si128(m0,1);
            }
            else{
                m1  =   _mm_srli_si128(m0,0);
            }

            refPos   = (refPos16_3 >> 4) - x_BL;
            refPos_3   = refPos - refPos_2;
            if(refPos){
            m2      =   _mm_srli_si128(m1,1);
            }
            else{
                m2  =   _mm_srli_si128(m1,0);
            }
            refPos_2   = (refPos16_4 >> 4) - x_BL;
            refPos_3   = refPos_2 - refPos;
            if(refPos_3){
            m3      =   _mm_srli_si128(m2,1);
            }
            else{
                m3  =   _mm_srli_si128(m2,0);
            }

            m0      =   _mm_unpacklo_epi64(m0,m1);
            m2      =   _mm_unpacklo_epi64(m2,m3);
            m10      =   _mm_maddubs_epi16(m0,m14);
            m2      =   _mm_maddubs_epi16(m2,m12);
            m10      =   _mm_hadd_epi16(m10,m2);


            refPos16 = temp[i+4];
            refPos16_2 = temp[i+5];
            refPos16_3 = temp[i+6];
            refPos16_4 = temp[i+7];


            refPos_3   = (refPos16 >> 4) - x_BL;


            refPos_2   = refPos - refPos_3;
            if(refPos_2){
            m0      =   _mm_srli_si128(m3,1);
            }
            else{
            m0  =   _mm_srli_si128(m3,0);
            }

            UPSAMPLE_L_LOAD_H();

            refPos   = (refPos16 >> 4) - x_BL;

            refPos_2   = (refPos16_2 >> 4) - x_BL;
            refPos_3   = refPos_2 - refPos;
            if(refPos_3){
            m1      =   _mm_srli_si128(m0,1);
            }
            else{
                m1  =   _mm_srli_si128(m0,0);
            }

            refPos_3   = (refPos16_3 >> 4) - x_BL;
            refPos   = refPos_3 - refPos_2;
            if(refPos){
            m2      =   _mm_srli_si128(m1,1);
            }
            else{
                m2  =   _mm_srli_si128(m1,0);
            }
            refPos   = (refPos16_4 >> 4) - x_BL;
            refPos_2   = refPos - refPos_3;
            if(refPos_2){
            m3      =   _mm_srli_si128(m2,1);
            }
            else{
                m3  =   _mm_srli_si128(m2,0);
            }

            m0      =   _mm_unpacklo_epi64(m0,m1);
            m2      =   _mm_unpacklo_epi64(m2,m3);

            m9      =   _mm_maddubs_epi16(m0,m14);
            m2      =   _mm_maddubs_epi16(m2,m12);
            m9      =   _mm_hadd_epi16(m9,m2);

            m10      =   _mm_hadd_epi16(m10,m9);
            _mm_storeu_si128(&dst_tmp[i],m10);

        }

    }
    av_freep(&temp);
}

void ff_upsample_filter_block_cr_h_8_sse( int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                   int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                                   const int8_t enabled_up_sample_filter_chroma[16][4], struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {


    int leftStartC = Enhscal->left_offset>>1;
    int rightEndC  = widthEL - (Enhscal->right_offset>>1);
    int x, i, j, phase, refPos16, refPos,refPos16_2, refPos_2,refPos_3;
    int16_t*  dst_tmp;
    uint8_t*  src_tmp;
    int8_t*  coeff;

    int * temp= NULL;
    __m128i     m0, m1, m2,m3,m4,m5, m6,m7,m8, r0,r1,r2,c0,c1,c2,c3;
    c0= _mm_setzero_si128();
            temp= av_malloc((block_w+16)*sizeof(int));
            m0= _mm_set_epi16(7,6,5,4,3,2,1,0);
            m1= _mm_set1_epi16(up_info->scaleXCr);
            m2= _mm_set1_epi32(up_info->addXCr);
            m3= _mm_set1_epi16(x_EL-leftStartC);            //!= from luma
            m4= _mm_set1_epi16(rightEndC-leftStartC);       //!= from luma
            m6= _mm_set1_epi16(8);

            UPSAMPLE_H_TABLE();

    m5= _mm_set_epi32(0,0,0,-1);
    for( j = 0; j < block_h ; j++ ) {
           src_tmp  = _src+_srcstride*j;
           dst_tmp  = dst + dststride*j;
           refPos= 0;
           for( i = 0; i < block_w-1; i+=2 ){
               refPos16 = temp[i];
               refPos16_2 = temp[i+1];

               coeff    = enabled_up_sample_filter_chroma[phase];
               refPos   = (refPos16 >> 4) - x_BL;

               c1       = _mm_loadl_epi64((__m128i *)&enabled_up_sample_filter_chroma[refPos16 & 15]);
               c1       = _mm_and_si128(c1,m5);
               c2       = _mm_loadl_epi64((__m128i *)&enabled_up_sample_filter_chroma[refPos16_2 & 15]);
               c2       = _mm_and_si128(c2,m5);
               c2       = _mm_slli_si128(c2,4);
               c1       = _mm_or_si128(c1,c2);

               refPos_2   = (refPos16_2 >> 4) - x_BL;
               refPos_3   = refPos_2 - refPos;

               m0      =   _mm_loadu_si128((__m128i *) (src_tmp+ refPos - 1));
               if(refPos_3){
               m1      =   _mm_srli_si128(m0,1);
               }
               else{
                   m1  =   _mm_srli_si128(m0,0);
               }
               m0      =   _mm_unpacklo_epi32(m0,m1);

               r0      =   _mm_maddubs_epi16(m0,c1);

               r0      =   _mm_hadd_epi16(r0,c0);

               _mm_maskmoveu_si128(r0, _mm_set_epi16(0,0,0,0,0,0,-1,-1),(char *)(dst_tmp+i));

           }
           refPos16 = temp[block_w-1];
           phase    = refPos16 & 15;
           coeff    = enabled_up_sample_filter_chroma[phase];
           refPos   = (refPos16 >> 4) - x_BL;

           c1       = _mm_loadl_epi64((__m128i *)&enabled_up_sample_filter_chroma[phase]);
           m0      =   _mm_loadl_epi64((__m128i *) (src_tmp+ refPos - 1));
           r0      =   _mm_maddubs_epi16(m0,c1);
           r0      =   _mm_hadd_epi16(r0,c0);

           _mm_maskmoveu_si128(r0, _mm_set_epi16(0,0,0,0,0,0,0,-1),(char *)&dst_tmp[block_w-1]);

       }
    av_freep(&temp);

}

void ff_upsample_filter_block_cr_h_8_8_sse( int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                   int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                                   const int8_t enabled_up_sample_filter_chroma[16][4], struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {


    int leftStartC = Enhscal->left_offset>>1;
    int rightEndC  = widthEL - (Enhscal->right_offset>>1);
    int x, i, j, phase, refPos16, refPos,refPos16_2, refPos_2,refPos_3,refPos16_3,refPos16_4;
    int16_t*  dst_tmp;
    uint8_t*  src_tmp;
    int8_t*  coeff;

    int * temp= NULL;
    __m128i     m0, m1, m2,m3,m4,m5, m6,m7,m8, r0,r1,r2,m15,m14,m13,m12,m11,m10;
    m15= _mm_setzero_si128();
            temp= av_malloc((block_w+16)*sizeof(int));
            m0= _mm_set_epi16(7,6,5,4,3,2,1,0);
            m1= _mm_set1_epi16(up_info->scaleXCr);
            m2= _mm_set1_epi32(up_info->addXCr);
            m3= _mm_set1_epi16(x_EL-leftStartC);            //!= from luma
            m4= _mm_set1_epi16(rightEndC-leftStartC);       //!= from luma
            m6= _mm_set1_epi16(8);

            UPSAMPLE_H_TABLE();

    m5= _mm_set_epi32(0,0,0,-1);
    for( j = 0; j < block_h ; j++ ) {
           src_tmp  = _src+_srcstride*j;
           dst_tmp  = dst + dststride*j;
           refPos= 0;
           for( i = 0; i < block_w; i+=8 ){
               refPos16 = temp[i];
               refPos16_2 = temp[i+1];
               refPos16_3 = temp[i+2];
               refPos16_4 = temp[i+3];

               coeff    = enabled_up_sample_filter_chroma[phase];
               refPos   = (refPos16 >> 4) - x_BL;

               m14       = _mm_loadl_epi64((__m128i *)&enabled_up_sample_filter_chroma[refPos16 & 15]);
               m14      = _mm_and_si128(m14,m5);
               m13       = _mm_loadl_epi64((__m128i *)&enabled_up_sample_filter_chroma[refPos16_2 & 15]);
               m13       = _mm_and_si128(m13,m5);
               m13       = _mm_slli_si128(m13,4);
               m14       = _mm_or_si128(m14,m13);

               m12       = _mm_loadl_epi64((__m128i *)&enabled_up_sample_filter_chroma[refPos16_3 & 15]);
               m12      = _mm_and_si128(m12,m5);
               m11       = _mm_loadl_epi64((__m128i *)&enabled_up_sample_filter_chroma[refPos16_4 & 15]);
               m11       = _mm_and_si128(m11,m5);
               m11       = _mm_slli_si128(m11,4);
               m12       = _mm_or_si128(m12,m11);

               m14      =   _mm_unpacklo_epi64(m14,m12);

               m0      =   _mm_loadu_si128((__m128i *) (src_tmp+ refPos - 1));

               refPos_2   = (refPos16_2 >> 4) - x_BL;
               refPos_3   = refPos_2 - refPos;
               if(refPos_3){
               m1      =   _mm_srli_si128(m0,1);
               }
               else{
               m1  =   _mm_srli_si128(m0,0);
               }

               refPos_3   = (refPos16_3 >> 4) - x_BL;
               refPos   = refPos_3 - refPos_2;
               if(refPos){
               m2      =   _mm_srli_si128(m1,1);
               }
               else{
               m2  =   _mm_srli_si128(m1,0);
               }

               refPos_2   = (refPos16_4 >> 4) - x_BL;
               refPos   = refPos_2 - refPos_3;
               if(refPos){
               m3      =   _mm_srli_si128(m2,1);
               }
               else{
               m3  =   _mm_srli_si128(m2,0);
               }
               m0      =   _mm_unpacklo_epi32(m0,m1);
               m2      =   _mm_unpacklo_epi32(m2,m3);
               m0      =   _mm_unpacklo_epi64(m0,m2);

               r0      =   _mm_maddubs_epi16(m0,m14);

               refPos16 = temp[i+4];
               refPos16_2 = temp[i+5];
               refPos16_3 = temp[i+6];
               refPos16_4 = temp[i+7];

               refPos   = (refPos16 >> 4) - x_BL;
               refPos_3   = refPos - refPos_2;
               if(refPos_3){
               m0      =   _mm_srli_si128(m3,1);
               }
               else{
               m0  =   _mm_srli_si128(m3,0);
               }

               m14       = _mm_loadl_epi64((__m128i *)&enabled_up_sample_filter_chroma[refPos16 & 15]);
               m14      = _mm_and_si128(m14,m5);
               m13       = _mm_loadl_epi64((__m128i *)&enabled_up_sample_filter_chroma[refPos16_2 & 15]);
               m13       = _mm_and_si128(m13,m5);
               m13       = _mm_slli_si128(m13,4);
               m14       = _mm_or_si128(m14,m13);

               m12       = _mm_loadl_epi64((__m128i *)&enabled_up_sample_filter_chroma[refPos16_3 & 15]);
               m12      = _mm_and_si128(m12,m5);
               m11       = _mm_loadl_epi64((__m128i *)&enabled_up_sample_filter_chroma[refPos16_4 & 15]);
               m11       = _mm_and_si128(m11,m5);
               m11       = _mm_slli_si128(m11,4);
               m12       = _mm_or_si128(m12,m11);

               m14      =   _mm_unpacklo_epi64(m14,m12);

               m0      =   _mm_loadu_si128((__m128i *) (src_tmp+ refPos - 1));

               refPos_2   = (refPos16_2 >> 4) - x_BL;
               refPos_3   = refPos_2 - refPos;
               if(refPos_3){
               m1      =   _mm_srli_si128(m0,1);
               }
               else{
               m1  =   _mm_srli_si128(m0,0);
               }

               refPos_3   = (refPos16_3 >> 4) - x_BL;
               refPos   = refPos_3 - refPos_2;
               if(refPos){
               m2      =   _mm_srli_si128(m1,1);
               }
               else{
               m2  =   _mm_srli_si128(m1,0);
               }

               refPos_2   = (refPos16_4 >> 4) - x_BL;
               refPos   = refPos_2 - refPos_3;
               if(refPos){
               m3      =   _mm_srli_si128(m2,1);
               }
               else{
               m3  =   _mm_srli_si128(m2,0);
               }

               m0      =   _mm_unpacklo_epi32(m0,m1);
               m2      =   _mm_unpacklo_epi32(m2,m3);
               m0      =   _mm_unpacklo_epi64(m0,m2);

               r1      =   _mm_maddubs_epi16(m0,m14);

               r0      =   _mm_hadd_epi16(r0,r1);

               _mm_storeu_si128(&dst_tmp[i],r0);


           }
       }
    av_freep(&temp);

}


void ff_upsample_filter_block_cr_v_8_sse(uint8_t *dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t _srcstride,
                                   int x_EL, int y_EL, int block_w, int block_h, int widthEL, int heightEL,
                                   const int8_t enabled_up_sample_filter_chroma[16][4], struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {
    int leftStartC = Enhscal->left_offset>>1;
    int rightEndC  = widthEL - (Enhscal->right_offset>>1);
    int topStartC  = Enhscal->top_offset>>1;
    int bottomEndC = heightEL - (Enhscal->bottom_offset>>1);
    int y, i, j, phase, refPos16, refPos;
    int8_t* coeff;
    int16_t *   src_tmp;
    uint8_t * dst_tmp;
    int refPos0 = av_clip_c(y_EL, topStartC, bottomEndC-1);

    refPos0     = (((( refPos0 - topStartC )* up_info->scaleYCr + up_info->addYCr) >> 12) -4 )>>4;

        __m128i     m0, m1, m2,m3,m4,m5, m6,m7,m8, m9, r0,r1,r2,c0,c1,c2,c3,c4;
        c0= _mm_setzero_si128();
        m9= _mm_set1_epi32(I_OFFSET);

    for( j = 0; j < block_h; j++ )  {
        y =   av_clip_c(y_EL+j - topStartC, 0, bottomEndC-1- topStartC);
        refPos16 =      ((( y  )* up_info->scaleYCr + up_info->addYCr) >> 12)-4;
        phase    = refPos16 & 15;
        coeff    = enabled_up_sample_filter_chroma[phase];
        refPos16 = (refPos16>>4);
        refPos   = refPos16 - refPos0;
        src_tmp  = _src  + refPos  * _srcstride;
        dst_tmp  =  dst  + y* dststride + x_EL;

        c1      =   _mm_loadl_epi64((__m128i *)coeff);
        c1      =   _mm_unpacklo_epi8(c0,c1);               //set to 16 bit
        c1      =   _mm_unpacklo_epi16(c0,c1);              //set to 32b
        c1      =   _mm_srai_epi32(c1,24);
        c2      =   _mm_srli_si128(c1,4);
        c3      =   _mm_srli_si128(c1,8);
        c4      =   _mm_srli_si128(c1,12);

        c1      =   _mm_shuffle_epi32(c1,0);                //bring to full register
        c2      =   _mm_shuffle_epi32(c2,0);
        c3      =   _mm_shuffle_epi32(c3,0);
        c4      =   _mm_shuffle_epi32(c4,0);

        for( i = 0; i < block_w-1; i+=2 )  {
            m1= _mm_loadl_epi64((__m128i*)(src_tmp-  _srcstride));
            m2= _mm_loadl_epi64((__m128i*)src_tmp);
            m3= _mm_loadl_epi64((__m128i*)(src_tmp+  _srcstride));
            m4= _mm_loadl_epi64((__m128i*)(src_tmp+2*_srcstride));

            m1= _mm_unpacklo_epi16(c0,m1);
            m2= _mm_unpacklo_epi16(c0,m2);
            m3= _mm_unpacklo_epi16(c0,m3);
            m4= _mm_unpacklo_epi16(c0,m4);

            m1= _mm_srai_epi32(m1,16);
            m2= _mm_srai_epi32(m2,16);
            m3= _mm_srai_epi32(m3,16);
            m4= _mm_srai_epi32(m4,16);

            m1= _mm_mullo_epi32(m1,c1);
            m2= _mm_mullo_epi32(m2,c2);
            m3= _mm_mullo_epi32(m3,c3);
            m4= _mm_mullo_epi32(m4,c4);

            m1= _mm_add_epi32(m1,m2);
            m3= _mm_add_epi32(m3,m4);
            m1= _mm_add_epi32(m1,m3);


            m1= _mm_add_epi32(m1,m9); //+ I_OFFSET

            m1= _mm_srli_epi32(m1,N_SHIFT); //shift

            m1= _mm_packus_epi32(m1,c0);
            m1= _mm_packus_epi16(m1,c0);

            *dst_tmp= _mm_extract_epi8(m1,0);

            if( ((x_EL+i) >= leftStartC) && ((x_EL+i) <= rightEndC-2) ){
                *(dst_tmp + 1)= _mm_extract_epi8(m1,1);
                if( ((x_EL+i+1) >= leftStartC) && ((x_EL+i+1) <= rightEndC-2) ){
                    src_tmp++;
                }
                src_tmp++;
            }
            else
                *(dst_tmp + 1)= _mm_extract_epi8(m1,0);

            dst_tmp+=2;
        }

        m1= _mm_loadl_epi64((__m128i*)(src_tmp-  _srcstride));
        m2= _mm_loadl_epi64((__m128i*)src_tmp);
        m3= _mm_loadl_epi64((__m128i*)(src_tmp+  _srcstride));
        m4= _mm_loadl_epi64((__m128i*)(src_tmp+2*_srcstride));

        m1= _mm_unpacklo_epi16(c0,m1);
        m2= _mm_unpacklo_epi16(c0,m2);
        m3= _mm_unpacklo_epi16(c0,m3);
        m4= _mm_unpacklo_epi16(c0,m4);

        m1= _mm_srai_epi32(m1,16);
        m2= _mm_srai_epi32(m2,16);
        m3= _mm_srai_epi32(m3,16);
        m4= _mm_srai_epi32(m4,16);

        m1= _mm_mullo_epi32(m1,c1);
        m2= _mm_mullo_epi32(m2,c2);
        m3= _mm_mullo_epi32(m3,c3);
        m4= _mm_mullo_epi32(m4,c4);

        m1= _mm_add_epi32(m1,m2);
        m3= _mm_add_epi32(m3,m4);
        m1= _mm_add_epi32(m1,m3);


        m1= _mm_add_epi32(m1,m9); //+ I_OFFSET

        m1= _mm_srli_epi32(m1,N_SHIFT); //shift

        m1= _mm_packus_epi32(m1,c0);
        m1= _mm_packus_epi16(m1,c0);

        *dst_tmp= _mm_extract_epi8(m1,0);

    }
}
#define UPSAMPLE_CR_V_COEFF()                                      \
c1      =   _mm_loadl_epi64((__m128i *)coeff);                      \
c1      =   _mm_unpacklo_epi8(c0,c1);                               \
c1      =   _mm_unpacklo_epi16(c0,c1);                              \
c1      =   _mm_srai_epi32(c1,24);                                  \
c2      =   _mm_srli_si128(c1,4);                                   \
c3      =   _mm_srli_si128(c1,8);                                   \
c4      =   _mm_srli_si128(c1,12);                                  \
c1      =   _mm_shuffle_epi32(c1,0);                                \
c2      =   _mm_shuffle_epi32(c2,0);                                \
c3      =   _mm_shuffle_epi32(c3,0);                                \
c4      =   _mm_shuffle_epi32(c4,0)

#define UPSAMPLE_CR_V_LOAD()                                       \
m1= _mm_loadl_epi64((__m128i*)(src_tmp-  _srcstride));              \
m2= _mm_loadl_epi64((__m128i*)src_tmp);                             \
m3= _mm_loadl_epi64((__m128i*)(src_tmp+  _srcstride));              \
m4= _mm_loadl_epi64((__m128i*)(src_tmp+2*_srcstride))

#define UPSAMPLE_COMPUTE_V_4()                                      \
m1= _mm_unpacklo_epi16(c0,m1);                                      \
m2= _mm_unpacklo_epi16(c0,m2);                                      \
m3= _mm_unpacklo_epi16(c0,m3);                                      \
m4= _mm_unpacklo_epi16(c0,m4);                                      \
m1= _mm_srai_epi32(m1,16);                                          \
m2= _mm_srai_epi32(m2,16);                                          \
m3= _mm_srai_epi32(m3,16);                                          \
m4= _mm_srai_epi32(m4,16);                                          \
m1= _mm_mullo_epi32(m1,c1);                                         \
m2= _mm_mullo_epi32(m2,c2);                                         \
m3= _mm_mullo_epi32(m3,c3);                                         \
m4= _mm_mullo_epi32(m4,c4);                                         \
m1= _mm_add_epi32(m1,m2);                                           \
m3= _mm_add_epi32(m3,m4);                                           \
m1= _mm_add_epi32(m1,m3);                                           \
m1= _mm_add_epi32(m1,m9);                                           \
m1= _mm_srli_epi32(m1,N_SHIFT);                                     \
m1= _mm_packus_epi32(m1,c0);                                        \
m1= _mm_packus_epi16(m1,c0)


#define UPSAMPLE_UNPACK_V()                                         \
m1= _mm_unpacklo_epi8(m1,m1);                                        \
m1= _mm_unpacklo_epi16(m1,m1);                                       \
m1= _mm_unpacklo_epi32(m1,m1)

#define UPSAMPLE_STORE_32()                                         \
        _mm_maskmoveu_si128(m1, m8,(char *)&dst_tmp[i])

void ff_upsample_filter_block_cr_v_8_8_sse(uint8_t *dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t _srcstride,
                                   int x_EL, int y_EL, int block_w, int block_h, int widthEL, int heightEL,
                                   const int8_t enabled_up_sample_filter_chroma[16][4], struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {
    int leftStartC = Enhscal->left_offset>>1;
    int rightEndC  = widthEL - (Enhscal->right_offset>>1);
    int topStartC  = Enhscal->top_offset>>1;
    int bottomEndC = heightEL - (Enhscal->bottom_offset>>1);
    int y, i, j, phase, refPos16, refPos;
    int8_t* coeff;
    int16_t *   src_tmp;
    uint8_t * dst_tmp;
    int refPos0 = av_clip_c(y_EL, topStartC, bottomEndC-1);
    int x;
    refPos0     = (((( refPos0 - topStartC )* up_info->scaleYCr + up_info->addYCr) >> 12) -4 )>>4;
    if((rightEndC-2 - x_EL) <= block_w - 1){
        x=rightEndC-2 - x_EL;
    }
    else
    {
        x= block_w - 1;
    }

    __m128i     m0, m1, m2,m3,m4,m5, m6,m7,m8, m9, r0,r1,r2,c0,c1,c2,c3,c4;
        m8= _mm_set_epi32(0,0,0,-1);
        c0= _mm_setzero_si128();
        m9= _mm_set1_epi32(I_OFFSET);

    for( j = 0; j < block_h; j++ )  {
        y =   av_clip_c(y_EL+j - topStartC, 0, bottomEndC-1- topStartC);
        refPos16 =      ((( y  )* up_info->scaleYCr + up_info->addYCr) >> 12)-4;
        phase    = refPos16 & 15;
        coeff    = enabled_up_sample_filter_chroma[phase];
        refPos16 = (refPos16>>4);
        refPos   = refPos16 - refPos0;
        src_tmp  = _src  + refPos  * _srcstride;
        dst_tmp  =  dst  + y* dststride + x_EL;

        UPSAMPLE_CR_V_COEFF();

        UPSAMPLE_CR_V_LOAD();
        UPSAMPLE_COMPUTE_V_4();
        UPSAMPLE_UNPACK_V();
        for(i = 0; i < leftStartC - x_EL;i+=4){
            UPSAMPLE_STORE_32();
        }
        for( i; i <= x; i+=4 )  {
            UPSAMPLE_CR_V_LOAD();
            UPSAMPLE_COMPUTE_V_4();
            UPSAMPLE_STORE_32();
            src_tmp+=4;
        }
        UPSAMPLE_CR_V_LOAD();
        UPSAMPLE_COMPUTE_V_4();
        UPSAMPLE_UNPACK_V();
        for(i;i<block_w;i+=4){
            UPSAMPLE_STORE_32();
        }
    }
}

#if 0
void ff_upsample_filter_block_luma_v_8_sse(uint8_t *dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t _srcstride,
                                      int x_EL, int y_EL, int block_w, int block_h, int widthEL, int heightEL,
                                      const int8_t enabled_up_sample_filter_luma[16][8], struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {
    int topStartL  = Enhscal->top_offset;
    int bottomEndL = heightEL - Enhscal->bottom_offset;
    int rightEndL  = widthEL - Enhscal->right_offset;
    int leftStartL = Enhscal->left_offset;

    int y, i, j, phase, refPos16, refPos;
    int8_t* coeff;
    int16_t *   src_tmp;
    uint8_t * dst_tmp;
    int refPos0 = av_clip_c(y_EL, topStartL, bottomEndL-1);
    __m128i     m0, m1, m2,m3,m4,m5, m6,m7,m8, m9, r0,r1,r2,c0,c1,c2,c3,c4,c5,c6,c7,c8;
    c0= _mm_setzero_si128();
    m9= _mm_set1_epi32(I_OFFSET);
    refPos0     = (((( refPos0 - topStartL )* up_info->scaleYLum + up_info->addYLum) >> 12) >> 4);
    for( j = 0; j < block_h; j++ )  {
        y        =   av_clip_c(y_EL+j, topStartL, bottomEndL-1);
        refPos16 = ((( y - topStartL )* up_info->scaleYLum + up_info->addYLum) >> 12);
        phase    = refPos16 & 15;
        coeff    = enabled_up_sample_filter_luma[phase];
        refPos   = (refPos16 >> 4) -refPos0;
        src_tmp  = _src  + refPos  * _srcstride;
        dst_tmp  =  dst  + (y_EL+j)* dststride + x_EL;

        UPSAMPLE_L_COEFF();

        for( i = 0; i < block_w-1; i+=2 )  {
            UPSAMPLE_L_LOAD();

            m1= _mm_unpacklo_epi16(c0,m1);
            m2= _mm_unpacklo_epi16(c0,m2);
            m3= _mm_unpacklo_epi16(c0,m3);
            m4= _mm_unpacklo_epi16(c0,m4);
            m5= _mm_unpacklo_epi16(c0,m5);
            m6= _mm_unpacklo_epi16(c0,m6);
            m7= _mm_unpacklo_epi16(c0,m7);
            m8= _mm_unpacklo_epi16(c0,m8);

            m1= _mm_srai_epi32(m1,16);
            m2= _mm_srai_epi32(m2,16);
            m3= _mm_srai_epi32(m3,16);
            m4= _mm_srai_epi32(m4,16);
            m5= _mm_srai_epi32(m5,16);
            m6= _mm_srai_epi32(m6,16);
            m7= _mm_srai_epi32(m7,16);
            m8= _mm_srai_epi32(m8,16);


            m1= _mm_mullo_epi32(m1,c1);
            m2= _mm_mullo_epi32(m2,c2);
            m3= _mm_mullo_epi32(m3,c3);
            m4= _mm_mullo_epi32(m4,c4);
            m5= _mm_mullo_epi32(m5,c5);
            m6= _mm_mullo_epi32(m6,c6);
            m7= _mm_mullo_epi32(m7,c7);
            m8= _mm_mullo_epi32(m8,c8);

            m1= _mm_add_epi32(m1,m2);
            m3= _mm_add_epi32(m3,m4);
            m1= _mm_add_epi32(m1,m3);
            m5= _mm_add_epi32(m5,m6);
            m7= _mm_add_epi32(m7,m8);
            m1= _mm_add_epi32(m1,m5);
            m1= _mm_add_epi32(m1,m7);

            m1= _mm_add_epi32(m1,m9); //+ I_OFFSET

            m1= _mm_srli_epi32(m1,N_SHIFT); //shift

            m1= _mm_packus_epi32(m1,c0);
            m1= _mm_packus_epi16(m1,c0);

            *dst_tmp= _mm_extract_epi8(m1,0);

            if( ((x_EL+i) >= leftStartL) && ((x_EL+i) <= rightEndL-2) ){
                            *(dst_tmp + 1)= _mm_extract_epi8(m1,1);
                            if( ((x_EL+i+1) >= leftStartL) && ((x_EL+i+1) <= rightEndL-2) ){
                                src_tmp++;
                            }
                            src_tmp++;
                        }
                        else
                            *(dst_tmp + 1)= _mm_extract_epi8(m1,0);

                        dst_tmp+=2;

        }
        UPSAMPLE_L_LOAD();

                    m1= _mm_unpacklo_epi16(c0,m1);
                    m2= _mm_unpacklo_epi16(c0,m2);
                    m3= _mm_unpacklo_epi16(c0,m3);
                    m4= _mm_unpacklo_epi16(c0,m4);
                    m5= _mm_unpacklo_epi16(c0,m5);
                    m6= _mm_unpacklo_epi16(c0,m6);
                    m7= _mm_unpacklo_epi16(c0,m7);
                    m8= _mm_unpacklo_epi16(c0,m8);

                    m1= _mm_srai_epi32(m1,16);
                    m2= _mm_srai_epi32(m2,16);
                    m3= _mm_srai_epi32(m3,16);
                    m4= _mm_srai_epi32(m4,16);
                    m5= _mm_srai_epi32(m5,16);
                    m6= _mm_srai_epi32(m6,16);
                    m7= _mm_srai_epi32(m7,16);
                    m8= _mm_srai_epi32(m8,16);


                    m1= _mm_mullo_epi32(m1,c1);
                    m2= _mm_mullo_epi32(m2,c2);
                    m3= _mm_mullo_epi32(m3,c3);
                    m4= _mm_mullo_epi32(m4,c4);
                    m5= _mm_mullo_epi32(m5,c5);
                    m6= _mm_mullo_epi32(m6,c6);
                    m7= _mm_mullo_epi32(m7,c7);
                    m8= _mm_mullo_epi32(m8,c8);

                    m1= _mm_add_epi32(m1,m2);
                    m3= _mm_add_epi32(m3,m4);
                    m1= _mm_add_epi32(m1,m3);
                    m5= _mm_add_epi32(m5,m6);
                    m7= _mm_add_epi32(m7,m8);
                    m1= _mm_add_epi32(m1,m5);
                    m1= _mm_add_epi32(m1,m7);

                    m1= _mm_add_epi32(m1,m9); //+ I_OFFSET

                    m1= _mm_srli_epi32(m1,N_SHIFT); //shift

                    m1= _mm_packus_epi32(m1,c0);
                    m1= _mm_packus_epi16(m1,c0);

                    *dst_tmp= _mm_extract_epi8(m1,0);
    }
}
#endif

#define UNPACK_SRAI16_4(inst, dst, src)                                        \
    dst ## 1 = _mm_srai_epi32(inst(c0, src ## 1), 16);                         \
    dst ## 2 = _mm_srai_epi32(inst(c0, src ## 2), 16);                         \
    dst ## 3 = _mm_srai_epi32(inst(c0, src ## 3), 16);                         \
    dst ## 4 = _mm_srai_epi32(inst(c0, src ## 4), 16)
#define UNPACK_SRAI16_8(inst, dst, src)                                        \
    dst ## 1 = _mm_srai_epi32(inst(c0, src ## 1), 16);                         \
    dst ## 2 = _mm_srai_epi32(inst(c0, src ## 2), 16);                         \
    dst ## 3 = _mm_srai_epi32(inst(c0, src ## 3), 16);                         \
    dst ## 4 = _mm_srai_epi32(inst(c0, src ## 4), 16);                         \
    dst ## 5 = _mm_srai_epi32(inst(c0, src ## 5), 16);                         \
    dst ## 6 = _mm_srai_epi32(inst(c0, src ## 6), 16);                         \
    dst ## 7 = _mm_srai_epi32(inst(c0, src ## 7), 16);                         \
    dst ## 8 = _mm_srai_epi32(inst(c0, src ## 8), 16)

#define MUL_ADD_V_4(mul, add, dst, src, coeff1,coeff2,coeff3,coeff4)           \
    dst = mul(src ## 1, coeff1);                                                \
    dst = add(dst, mul(src ## 2, coeff2));                                      \
    dst = add(dst, mul(src ## 3, coeff3));                                      \
    dst = add(dst, mul(src ## 4, coeff4))

#define UPSAMPLE_L_COEFF()                                         \
c1      =   _mm_loadl_epi64((__m128i *)coeff);                      \
c1      =   _mm_unpacklo_epi8(c0,c1);                               \
c5      =   _mm_unpackhi_epi16(c0,c1);                              \
c1      =   _mm_unpacklo_epi16(c0,c1);                              \
c1      =   _mm_srai_epi32(c1,24);                                  \
c2      =   _mm_srli_si128(c1,4);                                   \
c3      =   _mm_srli_si128(c1,8);                                   \
c4      =   _mm_srli_si128(c1,12);                                  \
c5      =   _mm_srai_epi32(c5,24);                                  \
c6      =   _mm_srli_si128(c5,4);                                   \
c7      =   _mm_srli_si128(c5,8);                                   \
c8      =   _mm_srli_si128(c5,12);                                  \
c1      =   _mm_shuffle_epi32(c1,0);                                \
c2      =   _mm_shuffle_epi32(c2,0);                                \
c3      =   _mm_shuffle_epi32(c3,0);                                \
c4      =   _mm_shuffle_epi32(c4,0);                                \
c5      =   _mm_shuffle_epi32(c5,0);                                \
c6      =   _mm_shuffle_epi32(c6,0);                                \
c7      =   _mm_shuffle_epi32(c7,0);                                \
c8      =   _mm_shuffle_epi32(c8,0)

#define UPSAMPLE_L_LOAD(src1, src2, src3,src4)                                         \
m1= _mm_loadu_si128((__m128i*)(src1));                                                  \
m2= _mm_loadu_si128((__m128i*)(src2));                                                  \
m3= _mm_loadu_si128((__m128i*)(src3));                                                  \
m4= _mm_loadu_si128((__m128i*)(src4));

#define UPSAMPLE_L_COMPUTE_LO_V(result,coeff1,coeff2,coeff3,coeff4)               \
UNPACK_SRAI16_4(_mm_unpacklo_epi16, m, m);          \
MUL_ADD_V_4(_mm_mullo_epi32, _mm_add_epi32, result, m, coeff1,coeff2,coeff3,coeff4)


#define UPSAMPLE_L_COMPUTE_ALL_V(resultlo,resulthi,coeff1,coeff2,coeff3,coeff4)               \
UNPACK_SRAI16_4(_mm_unpackhi_epi16, t, m);          \
MUL_ADD_V_4(_mm_mullo_epi32, _mm_add_epi32, resulthi, t, coeff1,coeff2,coeff3,coeff4); \
UPSAMPLE_L_COMPUTE_LO_V(resultlo, coeff1,coeff2,coeff3,coeff4)



void ff_upsample_filter_block_luma_v_8_8_sse(uint8_t *dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t _srcstride,
                                      int x_EL, int y_EL, int block_w, int block_h, int widthEL, int heightEL,
                                      const int8_t enabled_up_sample_filter_luma[16][8], struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {
    int topStartL  = Enhscal->top_offset;
    int bottomEndL = heightEL - Enhscal->bottom_offset;
    int rightEndL  = widthEL - Enhscal->right_offset;
    int leftStartL = Enhscal->left_offset;

    int y, i, j, phase, refPos16, refPos;
    int x;
    int8_t* coeff;
    int16_t *   src_tmp;
    uint8_t * dst_tmp;
    int refPos0 = av_clip_c(y_EL, topStartL, bottomEndL-1);
    __m128i     m0, m1, m2,m3,m4,m5, m6,m7,m8, m9, r0,r1,r2, r3,c0,c1,c2,c3,c4,c5,c6,c7,c8;
    __m128i     t1,t2,t3,t4;
    c0= _mm_setzero_si128();
    m9= _mm_set1_epi32(I_OFFSET);
    refPos0     = (((( refPos0 - topStartL )* up_info->scaleYLum + up_info->addYLum) >> 12) >> 4);

    if((rightEndL-2 - x_EL) <= block_w-1){
        x=rightEndL-2 - x_EL;
    }
    else
    {
        x= block_w-1;
    }

    for( j = 0; j < block_h; j++ )  {
        y        =   av_clip_c(y_EL+j, topStartL, bottomEndL-1);
        refPos16 = ((( y - topStartL )* up_info->scaleYLum + up_info->addYLum) >> 12);
        phase    = refPos16 & 15;
        coeff    = enabled_up_sample_filter_luma[phase];
        refPos   = (refPos16 >> 4) -refPos0;
        src_tmp  = _src  + refPos  * _srcstride;
        dst_tmp  =  dst  + (y_EL+j)* dststride + x_EL;

        UPSAMPLE_L_COEFF();

       UPSAMPLE_L_LOAD(src_tmp- 3*_srcstride,src_tmp- 2*_srcstride,src_tmp- _srcstride,src_tmp);
       UPSAMPLE_L_COMPUTE_LO_V(r1,c1,c2,c3,c4);

       UPSAMPLE_L_LOAD(src_tmp + _srcstride,src_tmp + 2*_srcstride,src_tmp +  3*_srcstride,src_tmp +  4*_srcstride);
       UPSAMPLE_L_COMPUTE_LO_V(r0,c5,c6,c7,c8);

        r0= _mm_add_epi32(r0,r1);
        m1= _mm_add_epi32(r0,m9); //+ I_OFFSET

        m1= _mm_srli_epi32(m1,N_SHIFT); //shift

        m1= _mm_packus_epi32(m1,c0);
        m1= _mm_packus_epi16(m1,c0);
        m1= _mm_unpacklo_epi8(m1,m1);
        m1= _mm_unpacklo_epi16(m1,m1);
        m1= _mm_unpacklo_epi32(m1,m1);
        m1= _mm_unpacklo_epi64(m1,m1);

        for( i = 0; i < leftStartL - x_EL; i+=8 )  {
            _mm_storel_epi64((__m128i *) &dst_tmp[i],m1);
        }

        for( i; i <= x; i+=8 ){

            UPSAMPLE_L_LOAD(src_tmp- 3*_srcstride,src_tmp- 2*_srcstride,src_tmp- _srcstride,src_tmp);
            UPSAMPLE_L_COMPUTE_ALL_V(r0,r1,c1,c2,c3,c4);

            UPSAMPLE_L_LOAD(src_tmp + _srcstride,src_tmp + 2*_srcstride,src_tmp +  3*_srcstride,src_tmp +  4*_srcstride);
            UPSAMPLE_L_COMPUTE_ALL_V(r2,r3,c5,c6,c7,c8);

            r0= _mm_add_epi32(r0,r2);
            r1= _mm_add_epi32(r1,r3);

            r0= _mm_add_epi32(r0,m9); //+ I_OFFSET
            r1= _mm_add_epi32(r1,m9); //+ I_OFFSET
            r0= _mm_srli_epi32(r0,N_SHIFT); //shift
            r1= _mm_srli_epi32(r1,N_SHIFT); //shift
            m1= _mm_packus_epi32(r0,r1);
            m1= _mm_packus_epi16(m1,c0);

            _mm_storel_epi64((__m128i *) &dst_tmp[i],m1);
            src_tmp+=8;
        }

        UPSAMPLE_L_LOAD(src_tmp- 3*_srcstride,src_tmp- 2*_srcstride,src_tmp- _srcstride,src_tmp);
        UPSAMPLE_L_COMPUTE_LO_V(r1,c1,c2,c3,c4);

        UPSAMPLE_L_LOAD(src_tmp + _srcstride,src_tmp + 2*_srcstride,src_tmp +  3*_srcstride,src_tmp +  4*_srcstride);
        UPSAMPLE_L_COMPUTE_LO_V(r0,c5,c6,c7,c8);

        r0= _mm_add_epi32(r0,r1);
        m1= _mm_add_epi32(r0,m9); //+ I_OFFSET

        m1= _mm_srli_epi32(m1,N_SHIFT); //shift

        m1= _mm_packus_epi32(m1,c0);
        m1= _mm_packus_epi16(m1,c0);
        m1= _mm_unpacklo_epi8(m1,m1);
        m1= _mm_unpacklo_epi16(m1,m1);
        m1= _mm_unpacklo_epi32(m1,m1);
        m1= _mm_unpacklo_epi64(m1,m1);

        for(i;i < block_w; i+=8){
            _mm_storel_epi64((__m128i *) &dst_tmp[i],m1);
        }
    }
}



#undef LumHor_FILTER
#undef LumCro_FILTER
#undef LumVer_FILTER
#undef CroVer_FILTER
#endif


