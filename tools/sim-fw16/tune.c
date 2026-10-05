// FW 1.6 by N7DDC (David Fainitski), public domain: the tuning functions,
// copied unchanged from main.c of FW 1.6, for the NG simulator only (never
// built for the tuner). Only change: get_swr() no longer calls Btn_short()
// on a button press (there are no buttons in the simulator).

#include "tune.h"

int tune_last;      // not used by FW 1.6, the simulator sets it

void atu_reset(){
   ind = 0;
   cap = 1;
   SW = 0;
   Relay_set(ind, cap, SW);
   return;
}
void get_swr(){
   unsigned int pwr_cnt = 150, tuneoff_cnt = 300;
   unsigned int swr_1, pwr_1, PWR_max = 0;
   char cnt;
   PWR = 0;
   SWR = 0;
   PWR_max = 0;
   //
   while(PWR<min_for_start || PWR>max_for_start){   // waiting for good power
      //
      if(B_short){
         SWR = 0;
         break;
      }
      if(B_xlong){
         //Btn_xlong();
         SWR = 0;
         break;
      }
      //
      swr_1 = 1000;
      for(cnt=5; cnt>0; cnt--){
         get_pwr();
         if(SWR<swr_1){
            swr_1 = SWR ;
            pwr_1 = PWR;
            Delay_us(500);
         }
         else{
             SWR = swr_1;
             PWR = pwr_1;
             break;
         }
      }
      //
      if(PWR>min_for_start & PWR<max_for_start)
         break;
      //
      if(pwr_cnt>0){
          pwr_cnt --;
          if(PWR>PWR_max)
              PWR_max = PWR;
      }
      else {
         if(PWR_max!=PWR_fixed_old) draw_power(PWR_max);
         PWR_fixed_old = PWR_max;
         PWR_max = 0;
         pwr_cnt = 50;
         if(tuneoff_cnt>0) tuneoff_cnt--;
         else { SWR = 0; break; }
      }
   }
   //  good power
   return;
}
void tune(void){
   int SWR_mem;
   char cap_mem, ind_mem;
   //
   get_swr();
   if(SWR<=120) return;
   subtune();
   get_swr();
   if(SWR<=120) return;
   SWR_mem = SWR;
   cap_mem = cap;
   ind_mem = ind;
   if(SW==1) SW = 0;
   else SW = 1;
   subtune();
   get_swr();
   if(SWR>SWR_mem){
      if(SW==1) SW = 0;
      else SW = 1;
      cap = cap_mem;
      ind = ind_mem;
      Relay_set(ind, cap, SW);
      get_swr();
   }
   if(SWR<=120) return;
   sharp_tune();
   get_swr();
   if(SWR==999)
      atu_reset();
   return;
}
void subtune(void){
   cap = 0;
   ind = 0;
   Relay_set(ind, cap, SW);
   get_swr();
   if(SWR<=120) return;
   coarse_tune();
   get_swr();
   if(SWR<=120) return;
   sharp_tune();
   return;
}
void coarse_tune(void){
   int SWR_mem1 = 10000, SWR_mem2 = 10000, SWR_mem3 = 10000;
   char ind_mem1, cap_mem1, ind_mem2, cap_mem2, ind_mem3, cap_mem3;
   coarse_cap();
   coarse_ind();
   get_swr();
   if(SWR<=120) return;
   SWR_mem1 = SWR;
   ind_mem1 = ind;
   cap_mem1 = cap;
   if(cap<=2 & ind<=2){
      cap = 0;
      ind = 0;
      Relay_set(ind, cap, SW);
      coarse_ind();
      coarse_cap();
      get_swr();
      if(SWR<=120) return;
      SWR_mem2 = SWR;
      ind_mem2 = ind;
      cap_mem2 = cap;
   }
   if(cap<=2 & ind<=2){
      cap = 0;
      ind = 0;
      Relay_set(ind, cap, SW);
      coarse_ind_cap();
      get_swr();
      if(SWR<=120) return;
      SWR_mem3 = SWR;
      ind_mem3 = ind;
      cap_mem3 = cap;
   }
   if(SWR_mem1<=SWR_mem2 & SWR_mem1<=SWR_mem3){
      cap = cap_mem1;
      ind = ind_mem1;
   }
   else if(SWR_mem2<=SWR_mem1 & SWR_mem2<=SWR_mem3){
      cap = cap_mem2;
      ind = ind_mem2;
   }
   else if(SWR_mem3<=SWR_mem1 & SWR_mem3<=SWR_mem2){
      cap = cap_mem3;
      ind = ind_mem3;
   }
   return;
}
void coarse_ind_cap(void){
   int SWR_mem;
   char ind_mem;
   ind_mem = 0;
   get_swr();
   SWR_mem = SWR / 10;
   for(ind=1; ind<64; ind*=2){
      Relay_set(ind, ind, SW);
      get_swr();
      SWR = SWR/10;
      if(SWR<=SWR_mem){
         ind_mem = ind;
         SWR_mem = SWR;
      }
      else
         break;
   }
   ind = ind_mem;
   cap = ind_mem;
   Relay_set(ind, cap, SW);
   return;
}
void coarse_cap(void){
   int SWR_mem;
   char cap_mem;
   cap_mem = 0;
   get_swr();
   SWR_mem = SWR / 10;
   for(cap=1; cap<64; cap*=2){
      Relay_set(ind, cap, SW);
      get_swr();
      SWR = SWR/10;
      if(SWR<=SWR_mem){
         cap_mem = cap;
         SWR_mem = SWR;
      }
      else
         break;
   }
   cap = cap_mem;
   Relay_set(ind, cap, SW);
   return;
}
void coarse_ind(void){
   int SWR_mem;
   char ind_mem;
   ind_mem = 0;
   get_swr();
   SWR_mem = SWR / 10;
   for(ind=1; ind<64; ind*=2){
      Relay_set(ind, cap, SW);
      get_swr();
      SWR = SWR/10;
      if(SWR<=SWR_mem){
         ind_mem = ind;
         SWR_mem = SWR;
      }
      else
         break;
   }
   ind = ind_mem;
   Relay_set(ind, cap, SW);
   return;
}
void sharp_tune(void){
   if(cap>=ind){
      sharp_cap();
      sharp_ind();
   }
   else{
      sharp_ind();
      sharp_cap();
   }
   return;
}
void sharp_cap(void){
   int SWR_mem;
   char step, cap_mem;
   cap_mem = cap;
   step = cap / 10;
   if(step==0) step = 1;
   get_swr();
   SWR_mem = SWR;
   cap += step;
   Relay_set(ind, cap, SW);
   get_swr();
   if(SWR<=SWR_mem){
      SWR_mem = SWR;
      cap_mem = cap;
      for(cap+=step; cap<=(127-step); cap+=step){
         Relay_set(ind, cap, SW);
         get_swr();
         if(SWR<=SWR_mem){
            cap_mem = cap;
            SWR_mem = SWR;
            step = cap / 10;
            if(step==0) step = 1;
         }
         else
            break;
      }
   }
   else{
      SWR_mem = SWR;
      for(cap-=step; cap>=step; cap-=step){
         Relay_set(ind, cap, SW);
         get_swr();
         if(SWR<=SWR_mem){
            cap_mem = cap;
            SWR_mem = SWR;
            step = cap / 10;
            if(step==0) step = 1;
         }
         else
            break;
      }
   }
   cap = cap_mem;
   Relay_set(ind, cap, SW);
   return;
}
void sharp_ind(void){
   int SWR_mem;
   char step, ind_mem;
   ind_mem = ind;
   step = ind / 10;
   if(step==0) step = 1;
   get_swr();
   SWR_mem = SWR;
   ind += step;
   Relay_set(ind, cap, SW);
   get_swr();
   if(SWR<=SWR_mem){
      SWR_mem = SWR;
      ind_mem = ind;
      for(ind+=step; ind<=(127-step); ind+=step){
         Relay_set(ind, cap, SW);
         get_swr();
         if(SWR<=SWR_mem){
            ind_mem = ind;
            SWR_mem = SWR;
            step = ind / 10;
            if(step==0) step = 1;
         }
         else
            break;
      }
   }
   else{
      SWR_mem = SWR;
      for(ind-=step; ind>=step; ind-=step){
         Relay_set(ind, cap, SW);
         get_swr();
         if(SWR<=SWR_mem){
            ind_mem = ind;
            SWR_mem = SWR;
            step = ind / 10;
            if(step==0) step = 1;
         }
         else
            break;
      }
   }
   ind = ind_mem;
   Relay_set(ind, cap, SW);
   return;
}
