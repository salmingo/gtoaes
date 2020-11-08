/* 
 * File:   gwac.h
 * Author: xy
 *
 * Created on September 9, 2013, 9:10 AM
 */

#ifndef GWAC_H
#define	GWAC_H


#include <iostream>
#include <vector>
#include <string>

using namespace std;

typedef struct {
  float x;
  float y;
  float ra;
  float dec;
  float flux;
  float mag;
  float fwhm;
  unsigned int pixnum;
  int flags;
  float thresh;
} ST_STAR;

typedef struct {
  ST_STAR obj;
  ST_STAR ref;
} ST_STARPEER;

#define RESIDUALS1
#define GWAC_TEST1

#define MAX_LINE_LENGTH 1024
#define DEGREE_TO_RADIANS 57.295779513
#define SECOND_TO_RADIANS 206264.806247

#define GWAC_SUCCESS 0
#define GWAC_ERROR 3001
#define GWAC_OPEN_FILE_ERROR 3002
#define GWAC_MALLOC_ERROR 3003
#define GWAC_FUNCTION_INPUT_NULL 3004
#define GWAC_FUNCTION_INPUT_EMPTY 3005
#define GWAC_STATUS_STR_NULL 3006
#define GWAC_STRING_NULL_OR_EMPTY 3007
#define GWAC_SEND_DATA_ERROR 3008

#define CHECK_RETURN_SATUS(var) {if(var!=GWAC_SUCCESS)return var;}
#define GWAC_REPORT_ERROR(errCode, errStr) \
        {sprintf(statusstr, "Error Code: %d\n"\
            "File %s line %d, %s\n", \
            errCode, __FILE__, __LINE__, errStr); \
            return errCode;}
#define CHECK_STRING_NULL_OR_EMPTY(var, varname) \
        {if(var==NULL || strcmp(var, "") == 0){\
            sprintf(statusstr, "Error Code: %d\n"\
                "File %s line %d, string \"%s\" is NULL or empty!\n", \
                GWAC_STRING_NULL_OR_EMPTY, __FILE__, __LINE__, varname);\
            return GWAC_FUNCTION_INPUT_NULL;}}
#define CHECK_STATUS_STR_IS_NULL(statusstr) \
        {if(statusstr==NULL){\
            printf("Error Code: %d\n"\
            "File %s line %d, statusstr is NULL\n", \
            GWAC_STATUS_STR_NULL, __FILE__, __LINE__); \
            return GWAC_STATUS_STR_NULL;}}
#define CHECK_INPUT_IS_NULL(var,varname) \
        {if(var==NULL){\
            sprintf(statusstr, "Error Code: %d\n"\
                "File %s line %d, the input parameter \"%s\" is NULL!\n", \
                GWAC_FUNCTION_INPUT_NULL, __FILE__, __LINE__, varname);\
            return GWAC_FUNCTION_INPUT_NULL;}}
#define MALLOC_IS_NULL() \
        {sprintf(statusstr, "Error Code: %d\n"\
                "File %s line %d, melloc memory error!\n", \
                GWAC_MALLOC_ERROR, __FILE__, __LINE__);\
            return GWAC_MALLOC_ERROR;}
#define CHECK_OPEN_FILE(fp,fname) \
        {if(fp==NULL){\
            sprintf(statusstr, "Error Code: %d\n"\
                "File %s line %d, open file \"%s\" error!\n", \
                GWAC_OPEN_FILE_ERROR, __FILE__, __LINE__, fname);\
            return GWAC_OPEN_FILE_ERROR;}}

int Gwac_geomap(vector<ST_STARPEER> matchpeervec,
        unsigned int order,
        unsigned int iter,
        float rejsigma,
        float &xrms,
        float &yrms,
        float &xrms2,
        float &yrms2,
        const char outfilename[],
        char statusstr[]);
int cofun(double x1,
        double x2,
        double *afunc,
        int cofNum);
int GetShift(const char transfilename[],
        float &xshift,
        float &yshift,
        char statusstr[]);
int Gwac_geoxytran(vector<ST_STAR> &objvec,
        const char transfilename[],
        int flag,
        char statusstr[]);
int Gwac_cctran(vector<ST_STAR> &objvec,
        const char transfilename[],
        int flag,
        char statusstr[]);

#endif	/* GWAC_H */

