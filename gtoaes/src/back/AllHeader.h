/*
 * AllHeader.h
 *
 *  Created on: 2013-10-11
 *      Author: chb
 */

#ifndef ALLHEADER_H_
#define ALLHEADER_H_


#include <sys/time.h>

#define MSGTYPE_LOG 1001

#ifndef GWAC_OK
#define GWAC_OK 0
#endif

#ifndef GWAC_UNDEFERR
#define GWAC_UNDEFERR -1
#endif

/*
#define ERROR_SEX 2001
#define ERROR_VECSIZE 2002    //vector 大小错误
#define ERROR_MALLOC  2003    //malloc错误
#define ERROR_FLAG   2004         //fileflag错误
#define ERROR_FILEOPEN   2005         //fileflag错误
#define ERROR_UNDEFTENUMTYPE   2006    //未定义枚举类型
#define ERROR_UNDEFKEY   2007    //未定义键值
#define ERROR_CHARARRAYSIZE 2008    //数组长度错误
//#define ERROR_FILEOPEN 2009      //文件无法打开
#define ERROR_FILEGET 2010      //文件无法读取
#define ERROR_FILEFORMAT 2011      //文件格式错误
#define ERROR_DIROPEN   2012     //文件夹无法打开

#define ERROR_PTSIZE 2013       //指针大小有误
#define ERROR_LESSOFINPUT 2014       //输入时间或角度字节数过小
#define ERROR_LITTLEIMGNUM  2015   //用于合并图像数太少
#define ERROR_FITSOPEN 2016          //fits文件open错误
#define ERROR_FITSIMGGET 2016          //fits文件imgget错误
#define ERROR_FITSROWSCOLS 2017       //fits文件行列数与预想不符
#define ERROR_PTERROR   2018          //指针错误
#define ERROR_REMOTECP  2019          //REMOTECP错误
*/



#define ERROR_SEX 2001
#define ERROR_VECSIZE 2002    //vector 大小错误
#define ERROR_MALLOC  2003    //malloc错误
#define ERROR_FLAG   2004         //fileflag错误
#define ERROR_FILEOPEN   2005         //fileflag错误
#define ERROR_UNDEFTENUMTYPE   2006    //未定义枚举类型
#define ERROR_UNDEFKEY   2007    //未定义键值
#define ERROR_CHARARRAYSIZE 2008    //数组长度错误
//#define ERROR_FILEOPEN 2009      //文件无法打开
#define ERROR_FILEGET 2010      //文件无法读取
#define ERROR_FILEFORMAT 2011      //文件格式错误
#define ERROR_DIROPEN   2012     //文件夹无法打开

#define ERROR_PTSIZE 2013       //指针大小有误
#define ERROR_LESSOFINPUT 2014       //输入时间或角度字节数过小
#define ERROR_LITTLEIMGNUM  2015   //用于合并图像数太少
#define ERROR_FITSOPEN 2016          //fits文件open错误
#define ERROR_FITSIMGGET 2017          //fits文件imgget错误
#define ERROR_FITSROWSCOLS 2018       //fits文件行列数与预想不符
#define ERROR_PTERROR   2019          //指针错误

#define ERROR_REMOTECP  2020          //REMOTECP错误
#define ERROR_READSEX  2021          //ReadSexTable错误
#define ERROR_LESSSEX  2022          //sex提取目标过少
#define ERROR_READSTAND  2023          //ReadStandTable错误
#define ERROR_SEXDelNearStar  2024          //DelNearStar错误
#define ERROR_STANDDelNearStar  2025          //DelNearStar错误
#define ERROR_XYMATCH  2026          //Gwac_xyxymatch错误
#define ERROR_GetFileName  2027          //GetFileNamePrefix错误
#define ERROR_GEOMAP  2028          //Gwac_geomap错误
#define ERROR_GEOXYTRAN  2029          //Gwac_geoxytran错误

#define ERROR_LARGEMESH  2030          //regionrows,regioncols错误
#define ERROR_READREFFILE  2031          //ReadStandTable错误
#define ERROR_CREATETRI  2032        //Create_tri错误
#define ERROR_TRIVOTE  2033        //vote_triangle错误
#define ERROR_TRIRMS  2034        //triangle rms过大
#define ERROR_GEOMAPRMS  2035       // Gwac_geomap rms过大
#define ERROR_ORDER 2050               //Gwac_singlefit order过小


#define ERROR_OTHER 2999


#define ERROR_ReadParamFile 2051       //ReadParamFile错误
#define ERROR_PathLength 2052          //PathofRevimg长度不够
#define ERROR_GetSavePath 2053          //GetSavePath错误
#define ERROR_NewFileNameLength 2054          //newfile name过长
#define ERROR_FILESHORTER 2055          //文件太小
#define ERROR_FILEPROCESS 2056         //文件被其它进程使用




enum ENUM_MSG{STATE,ERROR,OTHER};
typedef struct
{
	ENUM_MSG msgtype;
	long msgmark;
	struct timeval timeval;
	char msgtext[256];
}ST_MSGBUF;


#endif /* ALLHEADER_H_ */
