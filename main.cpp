/*
Abstract:
This is the main c+ file for the Sleep Pattern Monitor project for Cornell
'18 SP CS3420/ECE3140 Final project done by Hyun Kyo Jung and Amy Jung Yun Park.
The application communicates with the user through switch interrupt and the LED matrix. 
The users are given a sleep score depending on the regularity and length of their sleeps.
*/

/*
Dependencies:
We made use of the Mbed online compiler and their libraries to control the 16 x 32 LED matrix.
Other than this file and structs.h file also uploaded in our project, other libraries were
pulled using Mbed.org online compiler. 
*/
#include "mbed.h"
#include "RGBmatrixPanel.h" // Hardware-specific library
#include "structs.h"
#include <stdio.h>
#include <time.h>
#include <string>
#include <stdlib.h> // atoi 


InterruptIn button(PTC6); // Switch 2 setup for interrupt
//Pin setup for matrix display
PinName ub1=PTB23; 
PinName ug1=PTA1;
PinName ur1=PTB9;
PinName lb2=PTC3;
PinName lg2=PTC2;
PinName lr2=PTA2;
RGBmatrixPanel matrix(ur1,ug1,ub1,lr2,lg2,lb2,PTB2,PTB3,PTB10,PTC12,PTB11,PTC4,false);

int min_slept = 0;
int sleepingStatus =0; //0 = Good sleep timing, 1= Need to go bed earlier, 2 = Need to go bed later, 3= Wake up earlier, 4 = Waker up later 
int volatile score = 50; //100 is the max, 0 is the min. Initially set to 50
int volatile num_bed_times = 0; //size of the bed time queue (keep tracks up to 14 days)
int volatile num_wake_times = 0; //size of the wake time queue (keep tracks up to 14 days)
enum User_State user_state = AWAKE;  //User is awake in the beginning.
bool start = true; //set to false after user first goes to bed (used to display description of our system in the very beginning)
r_time * last_bed_time = NULL; //bed time queue head
r_time * last_wake_time = NULL; //wake time queue head
r_time * avg_bed_time = NULL; //current average bed time
r_time * avg_wake_time = NULL; //current average wake time

//helper function that returns the pointer to the newly formed r_time struct
r_time * init_r_time(int hr, int min, int sec, int sum_sec, r_time * next){
    r_time * new_time = (r_time*) malloc(sizeof(r_time));
    new_time -> hr = hr;
    new_time -> min = min;
    new_time -> sec = sec;
    new_time -> sum_sec = sum_sec;
    new_time -> next = next;
    return new_time;
}

/*The main function that deals with wake-ups
0. retrieve current time
1. create a new r_time struct for the wake-up
2. update score by calculating the amount of hours the user slept 
3. update score by comparing the wake-up time and average wake-up time
4. update the average wake-up time. (Making sure that average wake-up time queue does not exceed 14 in size)
*/
void wake_time(void){
    //get the current relative time to start of the program.
    time_t seconds = time(NULL);
    string time_in_sec = ctime(&seconds);
    string h = time_in_sec.substr(11,2);
    string m = time_in_sec.substr(14,2);
    string s = time_in_sec.substr(17,2);
    
    int hr = atoi( h.c_str() );
    int min  = atoi( m.c_str() );
    int sec  = atoi( s.c_str() );

    int sum_sec = (hr * 60 * 60) + min * 60 + sec; 
    
    //create a new wake_time
    r_time * new_wake_time = init_r_time(hr, min, sec, sum_sec, NULL);
    num_wake_times++;
    
    //updating the score for the amount of sleep you got.
    r_time * copy_oldest_bed_time = last_bed_time;
    while (copy_oldest_bed_time->next != NULL){
        copy_oldest_bed_time = copy_oldest_bed_time->next;
    }
    if (sum_sec < copy_oldest_bed_time->sum_sec) sum_sec += 86400;
    min_slept = (sum_sec - copy_oldest_bed_time->sum_sec) / 60;
    if (min_slept < 300) {score -= 20 ;}
    else if (min_slept < 360) {score -= 10;}
    else if (min_slept < 420) {score = score;}
    else if (min_slept < 480) {score += 10;}
    else {score += 15;}
    
    //first time ever 
    if (last_wake_time->hr == 60){
        avg_wake_time = init_r_time(new_wake_time->hr, new_wake_time->min, new_wake_time->sec,new_wake_time->sum_sec,NULL);
        last_wake_time = new_wake_time;
    }
    
    //usual operation
    else {
        //comparing with the average wake time
        int withoutAbs = avg_wake_time->sum_sec - new_wake_time->sum_sec;
        int diff_sec = abs(avg_wake_time->sum_sec - new_wake_time->sum_sec);
        int diff_min = diff_sec / 60;
        
        //updating the score
        if (diff_min < 10) {
            score += 10;
            sleepingStatus = 0;
        }
        else if (diff_min < 30){
            score += 5;
            sleepingStatus = 0;
        } 
        else if (diff_min < 60){
            score = score;
            sleepingStatus = 0;
        }
        else if (diff_min < 90){
            score -= 5;
            if(withoutAbs<0){
                sleepingStatus = 3;
            } else{
                sleepingStatus = 4;
            }
        } 
        else if (diff_min < 120){
            score -= 10;
            if(withoutAbs<0){
                sleepingStatus = 3;
            } else{
                sleepingStatus = 4;
            }
        } 
        else {
             score -= 20;
            if(withoutAbs<0){
                sleepingStatus = 3;
            } else{
                sleepingStatus = 4;
            }           
        }
        
        //user went to bed after Midnight! Gotta do this conversion in case the average bed time is before midnight.
        if (new_wake_time->hr < 12){
            new_wake_time->hr+=24;}
        //user goes usually to bed after Midnight! Gotta do this conversion in case the last night's bed time is before midnight.
        if (avg_wake_time->hr < 12){
            avg_wake_time->hr += 24;}
        
        //updating the average wake time (we include the last 2 weeks)
        if (num_wake_times == 15){
            avg_wake_time->sum_sec = (avg_wake_time->sum_sec * (num_wake_times-1) - last_wake_time->sum_sec + new_wake_time->sum_sec) / 14;
        } else {
            avg_wake_time->sum_sec = (avg_wake_time->sum_sec * (num_wake_times-1) + new_wake_time->sum_sec) / num_wake_times;
        }
        avg_wake_time->sec =  avg_wake_time->sum_sec % 60;
        avg_wake_time->min = (avg_wake_time->sum_sec / 60) % 60;
        avg_wake_time->hr  = (avg_wake_time->sum_sec / 60) / 60;
        
        //gotta switch it back to normal 
        if (new_wake_time->hr > 24){
            new_wake_time->hr -= 24;}
        if (avg_wake_time->hr > 24){
            avg_wake_time->hr -= 24;}
        
        //push the new wake_time to the tail of the wake_time_queue
        r_time * copy_oldest_wake_time = last_wake_time;
        while (copy_oldest_wake_time->next != NULL){
            copy_oldest_wake_time = copy_oldest_wake_time->next;
        }
        //push new wake time to the tail
        copy_oldest_wake_time->next = new_wake_time;
        
        //if the queue is full, pop the oldest wake time.
        if (num_wake_times == 15){
            r_time copy_last_wake_time = *last_wake_time;
            last_wake_time = last_wake_time->next;
            free(&copy_last_wake_time);
            num_wake_times--;
        }
    }
    //set the score boundaries
    if (score > 100) score = 100;
    else if (score < 0) score = 0;
}

/*The main function that deals with going to sleep
0. retrieve current time
1. create a new r_time struct for the bed-time
2. update score by comparing the bed-time and average bed-time
3. update the average bed-time. (Making sure that average bed-time queue does not exceed 14 in size)
*/
void bed_time(void){
    //get the current relative time to start of the program.
    time_t seconds = time(NULL);
    string time_in_sec = ctime(&seconds);
    string h = time_in_sec.substr(11,2);
    string m = time_in_sec.substr(14,2);
    string s = time_in_sec.substr(17,2);
    
    int hr = atoi( h.c_str() );
    int min  = atoi( m.c_str() );
    int sec  = atoi( s.c_str() );

    int sum_sec = (hr * 60 * 60) + min * 60 + sec; 
    
    //create a new bed_time
    r_time * new_bed_time = init_r_time(hr, min, sec, sum_sec, NULL);
    num_bed_times++;
    
    //first time ever 
    if (last_bed_time->hr == 60){
        avg_bed_time = init_r_time(new_bed_time->hr, new_bed_time->min, new_bed_time->sec,new_bed_time->sum_sec,NULL);
        last_bed_time = new_bed_time;
        start = false;
   }
    
    //usual operation
    else {
        //comparing with the average bed time
        int withoutAbs = avg_bed_time->sum_sec - new_bed_time->sum_sec;
        int diff_sec = abs(avg_bed_time->sum_sec - new_bed_time->sum_sec);
        int diff_min = diff_sec / 60;

        //updating the score for different from normal
        if (diff_min < 10) {
            score += 10;
            sleepingStatus = 0;
        }
        else if (diff_min < 30){
            score += 5;
            sleepingStatus = 0;
        } 
        else if (diff_min < 60){
            score = score;
            sleepingStatus = 0;
        }
        else if (diff_min < 90){
            score -= 5;
            if(withoutAbs<0){
                sleepingStatus = 1;
            } else{
                sleepingStatus = 2;
            }
        }
        else if (diff_min < 120){
            score -= 10;
            if(withoutAbs<0){
                sleepingStatus = 1;
            } else{
                sleepingStatus = 2;
            }
        }
        else {
             score -= 20;
            if(withoutAbs<0){
                sleepingStatus = 1;
            } else{
                sleepingStatus = 2;
            }           
        }
        
        //set the score boundaries
        if (score > 100) score = 100;
        else if (score < 0) score = 0;
        
        
        //user went to bed after Midnight! Gotta do this conversion in case the average bed time is before midnight.
        if (new_bed_time->hr < 12){
            new_bed_time->hr+=24;}
        //user goes usually to bed after Midnight! Gotta do this conversion in case the last night's bed time is before midnight.
        if (avg_bed_time->hr < 12){
            avg_bed_time->hr += 24;}
        
        //updating the average bed time's sum_sec (we include the last 2 weeks)
        if (num_bed_times == 15){
            avg_bed_time->sum_sec = (avg_bed_time->sum_sec * (num_bed_times-1) - last_bed_time->sum_sec + new_bed_time->sum_sec) / 14;
        } else {
            avg_bed_time->sum_sec = (avg_bed_time->sum_sec * (num_bed_times-1) + new_bed_time->sum_sec) / num_bed_times;
        }
        avg_bed_time->sec =  avg_bed_time->sum_sec % 60;
        avg_bed_time->min = (avg_bed_time->sum_sec / 60) % 60;
        avg_bed_time->hr  = (avg_bed_time->sum_sec / 60) / 60;
        
        //gotta switch it back to normal 
        if (new_bed_time->hr > 24){
            new_bed_time->hr -= 24;}
        if (avg_bed_time->hr > 24){
            avg_bed_time->hr -= 24;}
        
        //push the new bed_time to the tail of the bed_time_queue
        r_time * copy_oldest_bed_time = last_bed_time;
        while (copy_oldest_bed_time->next != NULL){
            copy_oldest_bed_time = copy_oldest_bed_time->next;
        }
        //push the new bed time to the tail
        copy_oldest_bed_time->next = new_bed_time;
        
        //if the queue is full, pop the oldest bed time.
        if (num_bed_times == 15){
            r_time copy_last_bed_time = *last_bed_time;
            last_bed_time = last_bed_time->next;
            free(&copy_last_bed_time);
            num_bed_times--;
        }
    }
}

//Change the color with which to display our main pattern depending on the current score
void checkScoreAndSetupLEDcolor(void){
    int tenth = score/10;
    switch(tenth){
        case (0):{
            matrix.setTextColor(matrix.Color333(7,0,0));
            break;
        }
          case (1):{
            matrix.setTextColor(matrix.Color333(3,0,0));
            break;
        }
            case (2):{
            matrix.setTextColor(matrix.Color333(1,1,0));
            break;
        }
            case (3):{
            matrix.setTextColor(matrix.Color333(1,1,0));
            break;
        }
            case (4):{
            matrix.setTextColor(matrix.Color333(0,1,0));
            break;
        }
            case (5):{
            matrix.setTextColor(matrix.Color333(0,1,0));
            break;
        }
            case (6):{
            matrix.setTextColor(matrix.Color333(0,1,0));
            break;
        }
            case (7):{
            matrix.setTextColor(matrix.Color333(0,1,1));
            break;
        }
            case (8):{
            matrix.setTextColor(matrix.Color333(0,1,1));
            break;
        }
            case (9):{
            matrix.setTextColor(matrix.Color333(0,0,3));
            break;
        }
            case (10):{
            matrix.setTextColor(matrix.Color333(0,0,7));
            break;
        }
    }
}

//Load messages to show user to communicate how one could improve his or her sleep pattern
void setUpLEDmessage(){
    matrix.fillScreen(matrix.Color333(0, 0, 0));
    matrix.setCursor(0, 0);   
    matrix.setTextSize(1);    
    switch(sleepingStatus){
            case(0):{
            //check if enough sleeping hours
                if(min_slept<420 && user_state == AWAKE){
                    matrix.writeString("Sleep more!");
                    for(int i =0; i<10000;i++){matrix.updateDisplay();}
                    break;
                }
                else{
                    matrix.writeString("Keep it up!");
                    for(int i =0; i<10000;i++){matrix.updateDisplay();}
                    break;
                }
            }
            case(1):{
            matrix.writeString("Go to Bed");
            for(int i =0; i<10000;i++){matrix.updateDisplay();} 
            matrix.fillScreen(matrix.Color333(0, 0, 0));
            matrix.updateDisplay();
            matrix.setCursor(2,5);
            matrix.setTextSize(1);
            matrix.writeString("EARLY");
            for(int i =0; i<10000;i++){matrix.updateDisplay();} 
            break;
        }
            case(2):{
            matrix.writeString("Go to Bed");
            for(int i =0; i<10000;i++){matrix.updateDisplay();} 
            matrix.fillScreen(matrix.Color333(0, 0, 0));
            matrix.updateDisplay();
            matrix.setCursor(2,5);
            matrix.setTextSize(1);
            matrix.writeString("LATER");
            for(int i =0; i<10000;i++){matrix.updateDisplay();} 
            break;
        }
            case(3):{                       
            matrix.writeString("Wake   up");
            for(int i =0; i<10000;i++){matrix.updateDisplay();} 
            matrix.fillScreen(matrix.Color333(0, 0, 0));
            matrix.updateDisplay();
            matrix.setCursor(2,5);
            matrix.setTextSize(1);
            matrix.writeString("EARLY");
            for(int i =0; i<10000;i++){matrix.updateDisplay();} 
            for(int i =0; i<10000;i++){matrix.updateDisplay();}
            if(min_slept<420){
                matrix.fillScreen(matrix.Color333(0, 0, 0));
                matrix.updateDisplay();
                matrix.setCursor(2,0);
                matrix.setTextSize(1);
                matrix.writeString("Also Sleep");
                for(int i =0; i<10000;i++){matrix.updateDisplay();} 
                matrix.fillScreen(matrix.Color333(0, 0, 0));
                matrix.updateDisplay();
                matrix.setCursor(2,0);
                matrix.setTextSize(1);
                matrix.writeString("MORE!!");
                for(int i =0; i<10000;i++){matrix.updateDisplay();}
            }
            break;
        }
            case(4):{
            matrix.writeString("Wake   up");
            for(int i =0; i<10000;i++){matrix.updateDisplay();} 
            matrix.fillScreen(matrix.Color333(0, 0, 0));
            matrix.updateDisplay();
            matrix.setCursor(2,5);
            matrix.setTextSize(1);
            matrix.writeString("LATER");
            for(int i =0; i<10000;i++){matrix.updateDisplay();} 
            for(int i =0; i<10000;i++){matrix.updateDisplay();}
            if(min_slept<420){
                matrix.fillScreen(matrix.Color333(0, 0, 0));
                matrix.updateDisplay();
                matrix.setCursor(2,0);
                matrix.setTextSize(1);
                matrix.writeString("Also Sleep");
                for(int i =0; i<10000;i++){matrix.updateDisplay();} 
                matrix.fillScreen(matrix.Color333(0, 0, 0));
                matrix.updateDisplay();
                matrix.setCursor(2,0);
                matrix.setTextSize(1);
                matrix.writeString("MORE!!");
                for(int i =0; i<10000;i++){matrix.updateDisplay();}
            }
            break;
        }
        }
        matrix.fillScreen(matrix.Color333(0, 0, 0));
        matrix.setCursor(6, 0);   
        matrix.setTextSize(2); 
        if (score==50){
          matrix.setTextColor(matrix.Color333(0,0,7));
          matrix.setCursor(2, 5);
          matrix.setTextSize(1); 
          matrix.putc('1');
          matrix.putc('0');
          matrix.putc('0');
          matrix.putc('!');
          for(int i =0; i<10000;i++){ matrix.updateDisplay();}

        } else{   
            int tens = score / 10 + '0';
            char char_tens = static_cast<char>(tens);        
            matrix.putc(char_tens);
            int ones = score % 10 + '0';
            char char_ones = static_cast<char>(ones);           
            matrix.putc(char_ones); 
            for(int i =0; i<10000;i++){ matrix.updateDisplay();}
        }
}

//Retrieves the current time and displays it in addition to avg_wake_time or avg_bed_time depending on the users' state.
void display_curr_and_avg(void){
    time_t seconds = time(NULL);
    string time_in_sec = ctime(&seconds);
    string h = time_in_sec.substr(11,2);
    string m = time_in_sec.substr(14,2);
    
    int hr = atoi( h.c_str() );
    int min  = atoi( m.c_str() );

    char buffer [20];
    char buffer2 [20];
    if (min / 10 == 0){
        if (user_state == ASLEEP) {
            sprintf (buffer , "Bed: %d:0%d", hr, min);
            if (avg_bed_time->min / 10 == 0){
                sprintf (buffer2, "Avg: %d:0%d", avg_bed_time->hr, avg_bed_time->min);
                }
            else {
                sprintf (buffer2, "Avg: %d:%d", avg_bed_time->hr, avg_bed_time->min);
                }
            }
        else {
            sprintf (buffer , "Wake:%d:0%d", hr, min);
            if (avg_wake_time->min / 10 == 0){
                sprintf (buffer2, "Avg: %d:0%d", avg_wake_time->hr, avg_wake_time->min);
                }
            else {
                sprintf (buffer2, "Avg: %d:%d", avg_wake_time->hr, avg_wake_time->min);
                }
            }
    } 
    else {
        if (user_state == ASLEEP) {
            sprintf (buffer , "Bed: %d:%d", hr, min);
            if (avg_bed_time->min / 10 == 0){
                sprintf (buffer2, "Avg: %d:0%d", avg_bed_time->hr, avg_bed_time->min);
                }
            else {
                sprintf (buffer2, "Avg: %d:%d", avg_bed_time->hr, avg_bed_time->min);
                }
            }
        else {
            sprintf (buffer , "Wake:%d:%d", hr, min);
            if (avg_wake_time->min / 10 == 0){
                sprintf (buffer2, "Avg: %d:0%d", avg_wake_time->hr, avg_wake_time->min);
                }
            else {
                sprintf (buffer2, "Avg: %d:%d", avg_wake_time->hr, avg_wake_time->min);
                }
            }
        }
    
    matrix.fillScreen(matrix.Color333(0,0,0));  
    matrix.setTextSize(1);    
    matrix.setCursor(0, 0); 
    matrix.writeString(buffer);
    for (int i = 0; i<20000; i++){matrix.updateDisplay();}
    matrix.fillScreen(matrix.Color333(0,0,0));  
    matrix.setCursor(0, 0); 
    matrix.writeString(buffer2);
    for (int i = 0; i<20000; i++){matrix.updateDisplay();}
}

//Raised when user presses switch 2 to go to bed or sleep
void InterruptHandler(void) {
    matrix.fillScreen(matrix.Color333(0, 0, 0));
    if (user_state == AWAKE){    //user is going to bed.
        bed_time();
        user_state = ASLEEP;
    } 
    else {                       //user is waking up.
        wake_time();
        user_state = AWAKE;
    }
    display_curr_and_avg();     //display current time and avg_bed_time or avg_wake_time depending on user's state
    checkScoreAndSetupLEDcolor();   //update the color for the main pattern to be displayed with new score
    setUpLEDmessage();              //load messages onto the LED matrix depending on how well the user did 
    //clearing the LED matrix 
    matrix.fillScreen(matrix.Color333(0, 0, 0));
    matrix.updateDisplay();  
}

//initial set up for interrupt, current time, matrix display color(White), and last bed, wake time
void initialize(void){     
    last_bed_time = init_r_time(60,60,60,0,NULL);
    last_wake_time = init_r_time(60,60,60,0,NULL);
    button.rise(&InterruptHandler);
    set_time(1526472060);                           //This has to be set by the user as the current time in the beginning of the time.
    matrix.setTextColor(matrix.Color333(1,1,1));
}

int main(){ 
    initialize();
    while(1) {      
            if (start){      
                //disable Interrupt
                __disable_irq();
                //intro
                matrix.setTextColor(matrix.Color333(1,1,1));
                matrix.setTextSize(1);    // size 1 == 8 pixels high
                for (int c = 32; c>-250; c--){
                    matrix.fillScreen(matrix.Color333(0,0,0));  
                    matrix.setCursor(c, 8);   
                    matrix.writeString("3420 Final Project By Amy & Hyun");
                    for (int i = 0; i<250; i++){matrix.updateDisplay();}
                }
                
                //project descriptions
                matrix.setTextColor(matrix.Color333(0,0,1));
                for (int c = 28; c>-750; c--){
                    matrix.fillScreen(matrix.Color333(0,0,0));  
                    matrix.setCursor(c, 8);   
                    matrix.writeString("Sleep Moni is a sleep pattern monitor. We give a score from 0 to 100 depending on 1. regularity of sleep & 2. amount of sleep");
                    for (int i = 0; i<250; i++){matrix.updateDisplay();}
                }
                //enalbe interrupt
                matrix.setTextColor(matrix.Color333(1,1,1));
                __enable_irq();
            }

            //main pattern
            for (int j =0; j < 10000; j++){matrix.updateDisplay(); }
                int r1 = 10;
                int r2 = 5;  
                int r3 = 0;
                int r4 = 2;            
                for (int i = 0; i < 1000; i ++){
                    matrix.fillScreen(matrix.Color333(0,0,0));  
                    matrix.setCursor(1, r1);   
                    matrix.setTextSize(1);   
                    matrix.putc('*');
                    matrix.setCursor(9, r2);   
                    matrix.setTextSize(1);    
                    matrix.putc('*');
                    matrix.setCursor(18, r3);   
                    matrix.setTextSize(1);    
                    matrix.putc('*');
                    matrix.setCursor(26, r4);   
                    matrix.setTextSize(1);    
                    matrix.putc('*');
                    for (int j =0; j < 1000; j++){matrix.updateDisplay(); }
                    r1 +=1;
                    r2 +=1;
                    r3 +=1;
                    r4 +=1;
                    if( r1>12){
                        r1 = 0;
                    }
                    if( r2>12){
                        r2 = 0;
                    }
                    if( r3>12){
                        r3 = 0;
                    }
                    if( r4>12){
                        r4 = 0;
                    }
                }  
            }
        }