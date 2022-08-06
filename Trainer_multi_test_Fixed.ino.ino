#include <LiquidCrystal.h>
#include <stdarg.h>
#include <EEPROM.h>
//The-Random-User1

//Session Settings
//==================================================

//universal settings
static const uint32_t ENGAGED_DISTANCE=9; //in aprox cm
static const int32_t SESSION_TIME=1200L; //session time In Seconds (for modes that end on a timer)
static const int32_t SAFETY_SESSION_TIME=3600L; //max safe session time.
static const bool STROKE_ON_FORWARD=false; //does a stroke count as going forward or backward?

//mode-specific settings
static const uint32_t STOPWATCH_MODE_TOTAL_ENGAGE_TIME_TARGET=500L; //the total amount of time in seconds you intend on needing to engage.
static const uint32_t COUNTING_MODE_TOTAL_STROKES_TARGET=200L;      //the total number of strokes to complete.
static const bool GOLF_MODE_SHOW_SCORE=false;


//===================================================


//bt-device API
void bt_release_key();
uint32_t bt_measure_distance();
bool bt_check_engaged(uint32_t distance);
bool bt_measure_engaged_strict(bool& is_engaged);
bool bt_detect_stroke(bool is_engaged);

void bt_set_feedback(bool engaged);
struct bt_HoursMinutesSeconds{ 
  int h,m,s;
  bt_HoursMinutesSeconds(int32_t ms);
};
int32_t bt_session_timer();
bool bt_safety_check();
bool bt_lcd_printf(int row,const char* fmt,...);


LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

//modes
void run_stopwatch_mode()
{
    const int32_t engaged_time_initial=STOPWATCH_MODE_TOTAL_ENGAGE_TIME_TARGET*1000L; //10 minutes; (TODO, config)
    const int32_t time_slice=100;
    int32_t engaged_time_left=engaged_time_initial;
    
    while(engaged_time_left > 0)
    {
    if(!bt_safety_check()) return;
    
        bool is_engaged;
    if(!bt_measure_engaged_strict(is_engaged)) { continue; } //if we're not certain about the engagement, start over the loop until we are..
    
    bt_set_feedback(is_engaged);
    
        if(is_engaged)
        {
            engaged_time_left-=time_slice;
        }
    
        lcd.clear();
        bt_lcd_printf(0,"%s","Time Left:");
    bt_HoursMinutesSeconds hms(engaged_time_left);
        bt_lcd_printf(1,"%02d:%02d:%02d",hms.h,hms.m,hms.s);

        delay(time_slice);
    }
}
void run_counting_mode()
{
  const int32_t session_strokes_expected=COUNTING_MODE_TOTAL_STROKES_TARGET;
    const int32_t time_slice=100;
    int32_t strokes_so_far=0;
    
    while(strokes_so_far < session_strokes_expected)
    {
    if(!bt_safety_check()) return;
    
    bool is_engaged;
    if(!bt_measure_engaged_strict(is_engaged)) { continue; } 
    
    bt_set_feedback(is_engaged);
        
    if(bt_detect_stroke(is_engaged))
    {
      strokes_so_far++;
    }
    
        lcd.clear();
        bt_lcd_printf(0,"Strokes:");
    bt_lcd_printf(1,"%ld/%ld",strokes_so_far,session_strokes_expected);
    
    delay(time_slice);
    }
}
void run_golf_mode()
{
  const int32_t session_time_initial=SESSION_TIME*1000L;
    const int32_t time_slice=100;
    int32_t session_time_left=session_time_initial;
  uint32_t current_score=0;
    
    while(session_time_left > 0)
    {
    if(!bt_safety_check()) return;
    
        uint32_t score=bt_measure_distance();
        bool is_engaged=bt_check_engaged(score);
    
    current_score+=score;
        session_time_left-=time_slice;
    
        bt_set_feedback(is_engaged);
        
    lcd.clear();
    bt_HoursMinutesSeconds hms(session_time_left);
        bt_lcd_printf(0,"Time:%02d:%02d:%02d",hms.h,hms.m,hms.s);
    if(GOLF_MODE_SHOW_SCORE)
        {
      uint32_t tscore=current_score*time_slice/1000UL;
      bt_lcd_printf(1,"Score:%lu",tscore);
    }
    
    delay(time_slice);
    }
  
  lcd.clear();
  bt_lcd_printf(0,"Final Score:");
  current_score*=time_slice;
  current_score/=1000;
  bt_lcd_printf(1,"%ld",current_score);
  delay(4000);
}

typedef void (*mode_func)();
struct mode_info_t{
    const char* name;
    const char* short_name;
    mode_func run_func;
};

mode_info_t modes[]={
    {"Stopwatch Mode","STOPWATCH",run_stopwatch_mode},
    {"Counting Mode","COUNTING",run_counting_mode},
    {"Golf Mode","GOLF",run_golf_mode}
};
static const unsigned int NUM_MODES=1+sizeof(modes)/sizeof(mode_info_t); //random+fixed modes

#define PIN_TRIG 6
#define PIN_ECHO 7
#define PIN_FEEDBACK_ENGAGED 8
#define PIN_FEEDBACK_DISENGAGED 9
#define PIN_RELEASE 13

#define ROTATE_MODE_TIME_LIMIT 3000

#define RANDSEED_ADDRESS 4
#define NEXT_MODE_ADDRESS 8

void setup_random();
//======================================================================
void setup() {
    lcd.begin(16, 2);
    // you can now interact with the LCD, e.g.:
    Serial.begin(9600);
    pinMode(PIN_TRIG, OUTPUT);
    pinMode(PIN_ECHO, INPUT);
    pinMode (PIN_FEEDBACK_DISENGAGED, OUTPUT);
    pinMode (PIN_FEEDBACK_ENGAGED, OUTPUT);
    pinMode (PIN_RELEASE, OUTPUT);
    digitalWrite(PIN_RELEASE, HIGH);
    
    uint32_t mode_select;
  EEPROM.get(NEXT_MODE_ADDRESS,mode_select);
  mode_select %= (uint32_t)NUM_MODES;
  EEPROM.put(NEXT_MODE_ADDRESS,mode_select+1);

  bool is_random_mode= (mode_select==(NUM_MODES-1));
  if(is_random_mode)
  {
    lcd.clear();
    bt_lcd_printf(0,"Selected Mode:");
    bt_lcd_printf(1,"RANDOM");
    delay(ROTATE_MODE_TIME_LIMIT);
    setup_random();
    EEPROM.put(NEXT_MODE_ADDRESS,mode_select); //write back the selection "random" so it stays there.
    mode_select=random(NUM_MODES-1);
  }
    
    mode_info_t* cur_mode=&modes[mode_select];
    
    lcd.clear();
    bt_lcd_printf(0,"Selected Mode:");
    bt_lcd_printf(1,"%s",cur_mode->short_name);
    delay(ROTATE_MODE_TIME_LIMIT);
  
  if(!is_random_mode) 
  {
    EEPROM.put(NEXT_MODE_ADDRESS,mode_select);
  }
  
  bt_session_timer();
    cur_mode->run_func();
}
//======================================================================
void loop() {
    bt_release_key();
    lcd.clear();
    bt_lcd_printf(0,"%s","RELEASED!");
    delay(1000);
}

void setup_random() //correctly initialize PRNG
{
  randomSeed(analogRead(0));
  int32_t cur_entropy=random(INT32_MAX);
  int32_t prev_entropy;
  EEPROM.get(RANDSEED_ADDRESS,prev_entropy);
  prev_entropy ^= cur_entropy;
  randomSeed(prev_entropy);
  int32_t next_seed=random(INT32_MAX);
  EEPROM.put(RANDSEED_ADDRESS,next_seed);
}

//low level interface
uint32_t bt_measure_distance(){
    // Start a new measurement:
    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);
    uint32_t reading=pulseIn(PIN_ECHO, HIGH);
    reading*=35; //approximately 34/2000 ~= 35/2048
    reading>>=11;
    return reading;
}

bool bt_measure_engaged_strict(bool& is_engaged){
  const unsigned int num_strict_tests=8;
  bool target=bt_check_engaged(bt_measure_distance());
  for(unsigned int i=0;i<(num_strict_tests-1);i++)
  {
    if(bt_check_engaged(bt_measure_distance()) != target) return false;
  }
  is_engaged=target;
  return true;
}

void bt_release_key()
{
    digitalWrite(PIN_RELEASE, LOW);
}
bool bt_check_engaged(uint32_t dist)
{
    return  dist <= ENGAGED_DISTANCE;
}
void bt_set_feedback(bool engaged)
{
    if(engaged)
    {
        digitalWrite(PIN_FEEDBACK_ENGAGED,LOW);
        digitalWrite(PIN_FEEDBACK_DISENGAGED,HIGH);
    }
    else
    {
        digitalWrite(PIN_FEEDBACK_ENGAGED,HIGH);
        digitalWrite(PIN_FEEDBACK_DISENGAGED,LOW);
    }
}
bt_HoursMinutesSeconds::bt_HoursMinutesSeconds(int32_t ms):
  h(ms/3600000L),
  m((ms % 3600000)/60000),
  s((ms % 60000) / 1000)
{}
int32_t bt_session_timer()
{
  static int32_t session_start=(int32_t)millis();
  return millis()-session_start;
}
bool bt_safety_check()
{
  bool is_safe=bt_session_timer() < (SAFETY_SESSION_TIME*1000L);
  if(!is_safe)
  {
    bt_lcd_printf(0,"%s","MAXIMUM SAFE");
    bt_lcd_printf(1,"%s","DURATION REACHED");
    delay(3000);
  }
  return is_safe;
}
bool bt_lcd_printf(int row,const char* fmt,...)
{
  char outbuf[18];
  va_list argptr;
    va_start(argptr, fmt);
    vsnprintf(outbuf, sizeof(outbuf),fmt, argptr);
    va_end(argptr);
  row &= 0x1; //mod 2
  lcd.setCursor(0,row);
  lcd.print(outbuf);
}
bool bt_detect_stroke(bool is_engaged)
{
  static bool last_state=is_engaged;
  if(last_state==is_engaged) return false; //if the state is the same, there's no movement
  last_state=is_engaged;
  return STROKE_ON_FORWARD == is_engaged; //otherwise there's movement.  The stroke is detected if we're looking for the state we're in.
}
