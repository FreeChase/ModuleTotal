#ifndef _CIRCLEBUF_X_H_
#define _CIRCLEBUF_X_H_

#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
    char *buffer;
    int size;
    volatile int head;
    volatile int tail;
} CircularBuffer;


void initializeBuffer(CircularBuffer * cBuffer, char * pbuf, int size);
int writeBuffer(CircularBuffer *cBuffer, char data);
int readBuffer(CircularBuffer *cBuffer, int *data);

#ifdef __cplusplus
}
#endif


#endif