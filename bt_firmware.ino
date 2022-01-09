#include <LiquidCrystal.h>
//The-Random-User1



LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
#define PIN_TRIG 6
#define PIN_ECHO 7
#define PIN_FEEDBACK_ENGAGED 8
#define PIN_FEEDBACK_DISENGAGED 9
#define PIN_RELEASE 13

typedef void (*mode_func)();
struct mode_info_t{
    const char* name;
    const char* short_name;
    mode_func run_func;
};

//modes
void run_timer_mode();
void run_counting_mode();
void run_golf_mode();

mode_info_t modes[]={
    {"Timer Mode","TIMER",run_timer_mode},
    {"Counting Mode","COUNTING",run_counting_mode},
    {"Golf Mode","GOLF",run_golf_mode}
};
static const int NUM_MODES=sizeof(modes)/sizeof(mode_info_t);

//low level API
void bt_release_key();
uint32_t bt_measure_distance();
static const uint32_t ENGAGED_DISTANCE=90; //90mm
bool bt_measure_engaged();
void bt_set_feedback(bool engaged);
void bt_print_time(int32_t ms);



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
    
    int mode_select=1;
    
    mode_select %= NUM_MODES;
    mode_info_t* cur_mode=&modes[mode_select];
    
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("Entering Mode");
    lcd.setCursor(2,1);
    lcd.print(cur_mode->short_name);
    delay(2000);
    
    cur_mode->run_func();
}
//======================================================================
void loop() {
    bt_release_key();
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("RELEASED!");
    delay(1000);
}
//modes
void run_timer_mode()
{
    const int32_t engaged_time_initial=600000L; //10 minutes; (TODO, config)
    const int32_t time_slice=100;
    int32_t engaged_time_left=engaged_time_initial;
    
    while(engaged_time_left > 0)
    {
        bool is_engaged=bt_measure_engaged();
        if(is_engaged)
        {
            engaged_time_left-=time_slice;
        }
        bt_set_feedback(is_engaged);
        lcd.clear();
        lcd.setCursor(2,0);
        lcd.print("Engaged Time Remaining:");
        lcd.setCursor(2,1);
        bt_print_time(engaged_time_left);

        delay(time_slice);
    }
}
void run_counting_mode()
{
    const int32_t session_strokes_expected=100;
    
    const int32_t time_slice=25;
    int32_t strokes_so_far=0;
    
    bool prev_engaged=false;
    
    while(strokes_so_far < session_strokes_expected)
    {
        bool is_engaged=bt_measure_engaged();
        if(is_engaged&&(is_engaged != prev_engaged))
        {
            strokes_so_far++;
        }
        bt_set_feedback(is_engaged);
        lcd.clear();
        lcd.setCursor(2,0);
        lcd.print("Strokes:");
        lcd.setCursor(2,1);
        lcd.print(strokes_so_far);
        lcd.print("/");
        lcd.print(session_strokes_expected);
        prev_engaged = is_engaged;
        delay(time_slice);
        delay(200);
    }
}
void run_golf_mode()
{
    const int32_t session_time_initial=600000L; //10 minutes; (TODO, config)
    const int32_t time_slice=100;
    int32_t session_time_left=session_time_initial;
    uint32_t current_score=0;
    
    while(session_time_left > 0)
    {
        uint32_t score=bt_measure_distance();
        bool is_engaged=bt_measure_engaged();
        
        bt_set_feedback(is_engaged);
        lcd.clear();
        lcd.setCursor(2,0);
        lcd.print("Time Left:");
        lcd.setCursor(2,1);
        bt_print_time(session_time_left);
        current_score+=score;
        session_time_left-=time_slice;
        delay(time_slice);
        
    }
    
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("Score:");
    lcd.setCursor(2,1);
    current_score*=time_slice;
    current_score/=1000;
    lcd.print(current_score);
    delay(5000);
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

void bt_release_key()
{
    digitalWrite(PIN_RELEASE, LOW);
}
bool bt_measure_engaged()
{
    return bt_measure_distance() <= ENGAGED_DISTANCE;
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
void bt_print_time(int32_t ms)
{
    int32_t hours=ms/3600000;
    int32_t minutes=(ms % 3600000) / 60000;
    int32_t seconds=(ms % 60000) / 1000;
    
    lcd.print(hours); //TODO formatting.
    lcd.print(":");
    lcd.print(minutes);
    lcd.print(":");
    lcd.print(seconds);
}
