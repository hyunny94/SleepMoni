
/*
This file will contain definitions of structs that we need
*/


#ifndef __STRUCTS_H__
#define __STRUCTS_H__


enum User_State {AWAKE = 1, ASLEEP = 0};

/*
*/
typedef struct rel_time {
                int hr;                     
                int min;            
                int sec;                                
                int sum_sec;
                struct rel_time * next; 
} r_time;




#endif
