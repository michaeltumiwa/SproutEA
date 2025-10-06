// NEWEST VERSION AS OF BEFORE MEETING HARRY OFFLINE. ADDED SOME IPMROVEMENTS TO SHOW RSI PARAMETERS BUT DIDNT SHOW ANYTHING IDK WHY.
// -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// SproutEA.mq5 
#property indicator_separate_window
#property indicator_buffers 2
#property indicator_plots   2

//--- plot settings
#property indicator_label1  "RSI_M5"
#property indicator_type1   DRAW_LINE
#property indicator_color1  clrBlue
#property indicator_style1  STYLE_SOLID
#property indicator_width1  2

#property indicator_label2  "RSI_H1"
#property indicator_type2   DRAW_LINE
#property indicator_color2  clrRed
#property indicator_style2  STYLE_SOLID
#property indicator_width2  2

//--- buffers for plotting
double rsiM5Buffer[];
double rsiH1Buffer[];

// ^^^ FOR INDICATOR ^^^



#include <Trade/Trade.mqh>
CTrade trade;

// Configurable inputs (to be found what would be the most optimal)
input double init_lot_size  = 0.01;
input double lot_size_multi = 1.5;

input int    init_dist      = 200;
input double dist_multi     = 1.5;

input int    tp_dist        = 100;

input int    timeout        = 15;
input int    stoploss_ticks = 0;


// Indicator handles
int handleSlowRSI;
int handleFastRSI;


double currentLot;
double currentDistance;

// Cooldown trackers
datetime lastBuyTime = 0;
datetime lastSellTime = 0;

//NEWEST OnInit Version
int OnInit() {

   // vvv for indicators vvv
   // link buffers
   SetIndexBuffer(0, rsiM5Buffer, INDICATOR_DATA);
   SetIndexBuffer(1, rsiH1Buffer, INDICATOR_DATA);
   // ^^^ for indicators ^^^
   
   // start with defaults
   currentLot = init_lot_size;
   currentDistance = init_dist;

   // look back in history (last 1 month for example)
   datetime since = TimeCurrent() - 86400*60; //86400 seconds * 30
   if(HistorySelect(since, TimeCurrent())) {
      int deals = HistoryDealsTotal();
      for(int i = deals-1; i >= 0; i--) {
         ulong ticket = HistoryDealGetTicket(i);
         if(HistoryDealGetString(ticket, DEAL_SYMBOL) != _Symbol) continue;
         if((int)HistoryDealGetInteger(ticket, DEAL_ENTRY) != DEAL_ENTRY_OUT) continue; // ensure CLOSED
         if((string)HistoryDealGetString(ticket, DEAL_SYMBOL) == _Symbol) {
            double volume = HistoryDealGetDouble(ticket, DEAL_VOLUME);
            double profit = HistoryDealGetDouble(ticket, DEAL_PROFIT);

            // restore lot sizing
            if(profit < 0) {
               currentLot = volume * lot_size_multi;
               currentDistance = currentDistance * dist_multi;
            } else {
               currentLot = init_lot_size;
               currentDistance = init_dist;
            }
            break;
         }
      }
   }
   
   handleSlowRSI = iRSI(_Symbol, PERIOD_H1, 14, PRICE_CLOSE);
   handleFastRSI = iRSI(_Symbol, PERIOD_M5, 14, PRICE_CLOSE);

   if(handleSlowRSI == INVALID_HANDLE || handleFastRSI == INVALID_HANDLE) {
      Print("Error creating RSI handles");
      return INIT_FAILED;
   }

   Print("SproutEA started running");
   return INIT_SUCCEEDED;
   
}

void OnDeinit(const int reason) {
   IndicatorRelease(handleSlowRSI);
   IndicatorRelease(handleFastRSI);
   Print("SproutEA stopped running");
}

void OnTick() {
   // Ensure this logic only runs once per new M5 bar
   static datetime lastBarTime = 0;
   datetime currentBarTime = iTime(_Symbol, PERIOD_M5, 0);
   
   /*
   if(currentBarTime == lastBarTime){
      return;
   }
   */
   // the codeblock above makes it so that not per tick but per candle close.
   
   lastBarTime = currentBarTime;

   // Get RSI values
   double slowRSI[1], fastRSI[1];
   if(CopyBuffer(handleSlowRSI, 0, 0, 1, slowRSI) < 0) return;
   if(CopyBuffer(handleFastRSI, 0, 0, 1, fastRSI) < 0) return;

   double h1RSI = slowRSI[0];
   double m5RSI = fastRSI[0];

   // Current time
   datetime now = TimeCurrent();

   //(NEW) Trade conditions
    if(h1RSI > 50 && m5RSI < 30) {
      if(TimeCurrent() - lastBuyTime >= timeout*60) {
         double ask = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
         double point = SymbolInfoDouble(_Symbol, SYMBOL_POINT);
         double slPrice;
         if(stoploss_ticks > 0) {
            slPrice = ask - stoploss_ticks * point; // fixed SL
         } else {
            slPrice = ask - currentDistance * point; // progressive SL
         }

         double tpPrice = ask + tp_dist*point;
   
         if(trade.Buy(currentLot, _Symbol, ask, slPrice, tpPrice, "RSI Buy")) {
            lastBuyTime = TimeCurrent();

         }
      }
   }
   
   if(h1RSI < 50 && m5RSI > 70) {
      if(TimeCurrent() - lastSellTime >= timeout*60) {
         double bid = SymbolInfoDouble(_Symbol, SYMBOL_BID);
         double point = SymbolInfoDouble(_Symbol, SYMBOL_POINT);
         //double slPrice = (sl > 0) ? bid + sl*point : 0;
         //double slPrice = (stoploss_ticks > 0) ? bid + currentDistance * point : 0;
         double slPrice;
         if(stoploss_ticks > 0) {
            slPrice = bid + stoploss_ticks * point; // fixed SL
         } else {
            slPrice = bid + currentDistance * point; // progressive SL
         }

         double tpPrice = bid - tp_dist*point;
   
         if(trade.Sell(currentLot, _Symbol, bid, slPrice, tpPrice, "RSI Sell")) {
            lastSellTime = TimeCurrent();
         }
      }
   }

   // Debug info
   Comment("H1 RSI: ", h1RSI,
           "\nM5 RSI: ", m5RSI,
           "\nLast Buy: ", lastBuyTime,
           "\nLast Sell: ", lastSellTime);
}

void OnTimer(){   
}


void OnTrade(){
}

// Called whenever a trade-related event happens
void OnTradeTransaction(const MqlTradeTransaction& trans,
                        const MqlTradeRequest& request,
                        const MqlTradeResult& result) {
   // Only react when a new deal is added
   if(trans.type != TRADE_TRANSACTION_DEAL_ADD)
      return;

   if(!HistorySelect(TimeCurrent() - 86400*2, TimeCurrent())) //load history of last 2 days
      return;

   ulong ticket = trans.deal;

   string sym = HistoryDealGetString(ticket, DEAL_SYMBOL); // Make sure correct symbol
   if(sym != _Symbol)
      return;

   // Make sure this deal is a CLOSED trade (DEAL_ENTRY_OUT)
   int entryType = (int)HistoryDealGetInteger(ticket, DEAL_ENTRY);
   if(entryType != DEAL_ENTRY_OUT)
      return;

   double volume = HistoryDealGetDouble(ticket, DEAL_VOLUME); //trade details (volume or lot)
   double profit = HistoryDealGetDouble(ticket, DEAL_PROFIT);

   // Martingale logic
   if(profit < 0) {
      currentLot      = volume * lot_size_multi;
      currentDistance = currentDistance * dist_multi;
   } else {
      currentLot      = init_lot_size;
      currentDistance = init_dist;
   }
}


/*
//NEW MODIFICATION/ADDITION: OnTradeTransaction. Whenever a trade closes, we check its results.
void OnTradeTransaction(const MqlTradeTransaction& trans,
                        const MqlTradeRequest& request,
                        const MqlTradeResult& result) {
   if(trans.type == TRADE_TRANSACTION_DEAL_ADD) {
      if(HistorySelect(TimeCurrent()-86400*1, TimeCurrent())) {
         ulong ticket = trans.deal;
         string sym = HistoryDealGetString(ticket, DEAL_SYMBOL);
         if(sym != _Symbol) return;

         double volume = HistoryDealGetDouble(ticket, DEAL_VOLUME);
         double profit = HistoryDealGetDouble(ticket, DEAL_PROFIT);

         if(profit < 0) {
            currentLot = volume * lot_size_multi;
            currentDistance = currentDistance * dist_multi;
         } else {
            currentLot = init_lot_size;
            currentDistance = init_dist;
         }
      }
   }
}
*/

// =======================================================
// ADD OnCalculate TO FEED BUFFERS. FOR INDICATORS
// =======================================================
int OnCalculate(const int rates_total,
                const int prev_calculated,
                const datetime &time[],
                const double &open[],
                const double &high[],
                const double &low[],
                const double &close[],
                const long &tick_volume[],
                const long &volume[],
                const int &spread[])
{
   // Copy full RSI series into buffers for plotting
   CopyBuffer(handleFastRSI, 0, 0, rates_total, rsiM5Buffer);
   CopyBuffer(handleSlowRSI, 0, 0, rates_total, rsiH1Buffer);

   return(rates_total);
}



double OnTester(){
   double ret=0.0;
   return(ret);
}


void OnTesterInit(){
}


void OnTesterPass(){
}


void OnTesterDeinit(){
}


void OnChartEvent(const int32_t id,
                  const long &lparam,
                  const double &dparam,
                  const string &sparam){

}


void OnBookEvent(const string &symbol){
}


// NEWEST VERSION (apparently uses martingale? but making too little trades. NExt improvement wiill be to do no stop loss first.)
// -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// SproutEA.mq5 
#include <Trade/Trade.mqh>
CTrade trade;

// Configurable inputs (to be found what would be the most optimal)
input double init_lot_size  = 0.01;
input double lot_size_multi = 1.5;
input int    init_dist      = 200;
input double dist_multi     = 1.5;
input int    tp_dist        = 100;
input int    timeout        = 15;
input int    stoploss_ticks = 0;


// Indicator handles
int handleSlowRSI;
int handleFastRSI;


double currentLot;
double currentDistance;

// Cooldown trackers
datetime lastBuyTime = 0;
datetime lastSellTime = 0;
a
//NEWEST OnInit Version
int OnInit() {
   // start with defaults
   currentLot = init_lot_size;
   currentDistance = init_dist;

   // look back in history (last 1 month for example)
   datetime since = TimeCurrent() - 86400*30;
   if(HistorySelect(since, TimeCurrent())) {
      int deals = HistoryDealsTotal();
      for(int i = deals-1; i >= 0; i--) {
         ulong ticket = HistoryDealGetTicket(i);
         if(HistoryDealGetString(ticket, DEAL_SYMBOL) != _Symbol) continue;
         if((int)HistoryDealGetInteger(ticket, DEAL_ENTRY) != DEAL_ENTRY_OUT) continue; // ensure CLOSED
         if((string)HistoryDealGetString(ticket, DEAL_SYMBOL) == _Symbol) {
            double volume = HistoryDealGetDouble(ticket, DEAL_VOLUME);
            double profit = HistoryDealGetDouble(ticket, DEAL_PROFIT);

            // restore lot sizing
            if(profit < 0) {
               currentLot = volume * lot_size_multi;
               currentDistance = currentDistance * dist_multi;
            } else {
               currentLot = init_lot_size;
               currentDistance = init_dist;
            }
            break;
         }
      }
   }
   
   handleSlowRSI = iRSI(_Symbol, PERIOD_H1, 14, PRICE_CLOSE);
   handleFastRSI = iRSI(_Symbol, PERIOD_M5, 14, PRICE_CLOSE);

   if(handleSlowRSI == INVALID_HANDLE || handleFastRSI == INVALID_HANDLE) {
      Print("Error creating RSI handles");
      return INIT_FAILED;
   }

   Print("SproutEA started running");
   return INIT_SUCCEEDED;
   
}

void OnDeinit(const int reason) {
   IndicatorRelease(handleSlowRSI);
   IndicatorRelease(handleFastRSI);
   Print("SproutEA stopped running");
}

void OnTick() {
   // Ensure this logic only runs once per new M5 bar
   static datetime lastBarTime = 0;
   datetime currentBarTime = iTime(_Symbol, PERIOD_M5, 0);
   
   /*
   if(currentBarTime == lastBarTime){
      return;
   }
   */
   // the codeblock above makes it so that not per tick but per candle close.
   
   lastBarTime = currentBarTime;

   // Get RSI values
   double slowRSI[1], fastRSI[1];
   if(CopyBuffer(handleSlowRSI, 0, 0, 1, slowRSI) < 0) return;
   if(CopyBuffer(handleFastRSI, 0, 0, 1, fastRSI) < 0) return;

   double h1RSI = slowRSI[0];
   double m5RSI = fastRSI[0];

   // Current time
   datetime now = TimeCurrent();

   //(NEW) Trade conditions
    if(h1RSI > 50 && m5RSI < 30) {
      if(TimeCurrent() - lastBuyTime >= timeout*60) {
         double ask = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
         double point = SymbolInfoDouble(_Symbol, SYMBOL_POINT);
         double slPrice;
         if(stoploss_ticks > 0) {
            slPrice = ask - stoploss_ticks * point; // fixed SL
         } else {
            slPrice = ask - currentDistance * point; // progressive SL
         }

         double tpPrice = ask + tp_dist*point;
   
         if(trade.Buy(currentLot, _Symbol, ask, slPrice, tpPrice, "RSI Buy")) {
            lastBuyTime = TimeCurrent();

         }
      }
   }
   
   if(h1RSI < 50 && m5RSI > 70) {
      if(TimeCurrent() - lastSellTime >= timeout*60) {
         double bid = SymbolInfoDouble(_Symbol, SYMBOL_BID);
         double point = SymbolInfoDouble(_Symbol, SYMBOL_POINT);
         //double slPrice = (sl > 0) ? bid + sl*point : 0;
         //double slPrice = (stoploss_ticks > 0) ? bid + currentDistance * point : 0;
         double slPrice;
         if(stoploss_ticks > 0) {
            slPrice = bid + stoploss_ticks * point; // fixed SL
         } else {
            slPrice = bid + currentDistance * point; // progressive SL
         }

         double tpPrice = bid - tp_dist*point;
   
         if(trade.Sell(currentLot, _Symbol, bid, slPrice, tpPrice, "RSI Sell")) {
            lastSellTime = TimeCurrent();
         }
      }
   }

   // Debug info
   Comment("H1 RSI: ", h1RSI,
           "\nM5 RSI: ", m5RSI,
           "\nLast Buy: ", lastBuyTime,
           "\nLast Sell: ", lastSellTime);
}

void OnTimer(){   
}


void OnTrade(){
}

//NEW MODIFICATION/ADDITION: OnTradeTransaction. Whenever a trade closes, we check its results.
void OnTradeTransaction(const MqlTradeTransaction& trans,
                        const MqlTradeRequest& request,
                        const MqlTradeResult& result) {
   if(trans.type == TRADE_TRANSACTION_DEAL_ADD) {
      if(HistorySelect(TimeCurrent()-86400*1, TimeCurrent())) {
         ulong ticket = trans.deal;
         string sym = HistoryDealGetString(ticket, DEAL_SYMBOL);
         if(sym != _Symbol) return;

         double volume = HistoryDealGetDouble(ticket, DEAL_VOLUME);
         double profit = HistoryDealGetDouble(ticket, DEAL_PROFIT);

         if(profit < 0) {
            currentLot = volume * lot_size_multi;
            currentDistance = currentDistance * dist_multi;
         } else {
            currentLot = init_lot_size;
            currentDistance = init_dist;
         }
      }
   }
}

double OnTester(){
   double ret=0.0;
   return(ret);
}


void OnTesterInit(){
}


void OnTesterPass(){
}


void OnTesterDeinit(){
}


void OnChartEvent(const int32_t id,
                  const long &lparam,
                  const double &dparam,
                  const string &sparam){

}


void OnBookEvent(const string &symbol){
}






// -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// AFTER REFACTOR ADDING RESTRUCTURING TO SOLVE RSI VALUE NOT SHOWING IN ARRAY. STATIC POSITION SIZING IN WHICH TP AND SL ARE FIXED ON 200 POINTS DIFFERENCE
#include <Trade/Trade.mqh>
CTrade trade;

// Indicator handles
int handleSlowRSI;
int handleFastRSI;

// Cooldown trackers
datetime lastBuyTime = 0;
datetime lastSellTime = 0;

int OnInit() { //Called when expert starts running
   handleSlowRSI = iRSI(_Symbol, PERIOD_H1, 14, PRICE_CLOSE);
   handleFastRSI = iRSI(_Symbol, PERIOD_M5, 14, PRICE_CLOSE);

   if(handleSlowRSI == INVALID_HANDLE || handleFastRSI == INVALID_HANDLE) {
      Print("Error creating RSI handles");
      return INIT_FAILED;
   }

   Print("SproutEA started running");
   return INIT_SUCCEEDED;
}

void OnDeinit(const int reason) {  //Called when expert stops running
   IndicatorRelease(handleSlowRSI);
   IndicatorRelease(handleFastRSI);
   Print("SproutEA stopped running");
}

void OnTick() {
   // Ensure this logic only runs once per new M5 bar
   static datetime lastBarTime = 0;
   datetime currentBarTime = iTime(_Symbol, PERIOD_M5, 0);
   if(currentBarTime == lastBarTime) return;
   lastBarTime = currentBarTime;

   // Get RSI values
   double slowRSI[1], fastRSI[1];
   if(CopyBuffer(handleSlowRSI, 0, 0, 1, slowRSI) < 0) return;
   if(CopyBuffer(handleFastRSI, 0, 0, 1, fastRSI) < 0) return;

   double h1RSI = slowRSI[0];
   double m5RSI = fastRSI[0];

   // Current time
   datetime now = TimeCurrent();

   // Trade conditions
   if(h1RSI > 50 && m5RSI < 30) {
      if(now - lastBuyTime >= 15*60) { // 15 min cooldown
         double ask = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
         double point = SymbolInfoDouble(_Symbol, SYMBOL_POINT);
         double sl = ask - 200 * point;
         double tp = ask + 100 * point;

         if(trade.Buy(0.01, _Symbol, ask, sl, tp, "RSI Buy")) {
            lastBuyTime = now;
            Print("BUY opened at ", ask, " | H1 RSI: ", h1RSI, " | M5 RSI: ", m5RSI);
         }
      } else {
         Print("Buy signal ignored, cooldown active");
      }
   }
   else if(h1RSI < 50 && m5RSI > 70) {
      if(now - lastSellTime >= 15*60) { // 15 min cooldown
         double bid = SymbolInfoDouble(_Symbol, SYMBOL_BID);
         double point = SymbolInfoDouble(_Symbol, SYMBOL_POINT);
         double sl = bid + 200 * point;
         double tp = bid - 100 * point;

         if(trade.Sell(0.01, _Symbol, bid, sl, tp, "RSI Sell")) {
            lastSellTime = now;
            Print("SELL opened at ", bid, " | H1 RSI: ", h1RSI, " | M5 RSI: ", m5RSI);
         }
      } else {
         Print("Sell signal ignored, cooldown active");
      }
   }

   // Debug info
   Comment("H1 RSI: ", h1RSI,
           "\nM5 RSI: ", m5RSI,
           "\nLast Buy: ", lastBuyTime,
           "\nLast Sell: ", lastSellTime);
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// OLD VERSION OF OnTick() before refactor. Reason of refactor: RSI results in array did not get outputted. EIther not filled in or output method is wrong.
/*
void OnTick(){ //Called every tick. Define mechanism every tick
   static datetime timestamp;
   Print("timestamp: ", timestamp);
   
   datetime time = iTime(_Symbol, PERIOD_CURRENT, 0);
   Print("time: ", time);
   
   if(timestamp != time){
      timestamp = time;
      
      // RSI Strat Here
      //---
          
      static int handleSlowRSI = iRSI(_Symbol, PERIOD_H1, 0, PRICE_CLOSE);
      double slowRSIArray[];
      CopyBuffer(handleSlowRSI, 0, 1, 2, slowRSIArray);
      ArraySetAsSeries(slowRSIArray, true);
      
      static int handleFastRSI = iRSI(_Symbol, PERIOD_M5, 0, PRICE_CLOSE);
      double fastRSIArray[];
      CopyBuffer(handleFastRSI, 0, 1, 2, fastRSIArray);
      ArraySetAsSeries(fastRSIArray, true);
      
      if(fastRSIArray[0] > slowRSIArray[0] && fastRSIArray[1] < slowRSIArray[1]){
         Print("Fast RSI is now > than slow RSI");
         double ask = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
         double sl = ask - 200 * SymbolInfoDouble(_Symbol, SYMBOL_POINT);
         double tp = ask + 100 * SymbolInfoDouble(_Symbol, SYMBOL_POINT);
         trade.Buy(0.01, _Symbol, ask, sl, tp, "This is a buy");
      };
      
      if(fastRSIArray[0] < slowRSIArray[0] && fastRSIArray[1] < slowRSIArray[1]){
         Print("Fast MA is now < than slow MA");
         double bid = SymbolInfoDouble(_Symbol, SYMBOL_BID);
         double sl = bid - 200 * SymbolInfoDouble(_Symbol, SYMBOL_POINT);
         double tp = bid + 100 * SymbolInfoDouble(_Symbol, SYMBOL_POINT);
         trade.Sell(0.01, _Symbol, bid, sl, tp, "This is a sell");
      };
      
      Comment("\nslowRSIArray[0]: ", slowRSIArray[0],
              "\nslowRSIArray[1]: ", slowRSIArray[1],
              "\nfastRSIArray[1]: ", fastRSIArray[1],
              "\nfastRSIArray[1]: ", fastRSIArray[1]
      );
      
      // RSI Strat Here
      
      //---
      
      
      
      // RSI Strat Here
      /*
      static int handleSlowMA = iMA(_Symbol, PERIOD_CURRENT, 200, 0, MODE_SMA, PRICE_CLOSE);
      double slowMAArray[];
      CopyBuffer(handleSlowMA, 0, 1, 2, slowMAArray);
      ArraySetAsSeries(slowMAArray, true);
      
      static int handleFastMA = iMA(_Symbol, PERIOD_CURRENT, 200, 0, MODE_SMA, PRICE_CLOSE);
      double fastMAArray[];
      CopyBuffer(handleFastMA, 0, 1, 2, fastMAArray);
      ArraySetAsSeries(fastMAArray, true);
      */
      
      /*
      
      if(fastMAArray[0] > slowMAArray[0] && fastMAArray[1] < slowMAArray[1]){
         Print("Fast MA is now > than slow MA");
         double ask = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
         double sl = ask - 200 * SymbolInfoDouble(_Symbol, SYMBOL_POINT);
         double tp = ask + 100 * SymbolInfoDouble(_Symbol, SYMBOL_POINT);
         trade.Buy(0.01, _Symbol, ask, sl, tp, "This is a buy");
      }
      
      if(fastMAArray[0] < slowMAArray[0] && fastMAArray[1] < slowMAArray[1]){
         Print("Fast MA is now < than slow MA");
         double bid = SymbolInfoDouble(_Symbol, SYMBOL_BID);
         double sl = bid - 200 * SymbolInfoDouble(_Symbol, SYMBOL_POINT);
         double tp = bid + 100 * SymbolInfoDouble(_Symbol, SYMBOL_POINT);
         trade.Sell(0.01, _Symbol, bid, sl, tp, "This is a sell");
      }
      
      Comment("\nslowMAArray[0]: ", slowMAArray[0],
              "\nslowMaArray[1]: ", slowMAArray[1],
              "\nfastMaArray[1]: ", fastMAArray[1],
              "\nfastMaArray[1]: ", fastMAArray[1]
      );
   }
}

*/

  
void OnTimer(){
   
}


void OnTrade(){

   
}


void OnTradeTransaction(const MqlTradeTransaction& trans,
                        const MqlTradeRequest& request,
                        const MqlTradeResult& result)
{

}


double OnTester(){
   double ret=0.0;

   return(ret);
}


void OnTesterInit(){
   
}


void OnTesterPass(){

}


void OnTesterDeinit(){

}


void OnChartEvent(const int32_t id,
                  const long &lparam,
                  const double &dparam,
                  const string &sparam){

}


void OnBookEvent(const string &symbol){

}


// YT Modified to RSI. Before adding logic and stuff. (BEFORE REFACTOR)
// Reason of refactor: RSI results in array did not get outputted. EIther not filled in or output method is wrong.
#include <Trade/Trade.mqh>
CTrade trade;

// Indicator handles
int handleSlowRSI;
int handleFastRSI;

// Cooldown trackers
datetime lastBuyTime = 0;
datetime lastSellTime = 0;

/*
int OnInit(){ //Called when expert starts running
   //EventSetTimer(60); // create timer
   
   Print("SproutEA started running");
   return(INIT_SUCCEEDED);
}
*/

int OnInit() {
   handleSlowRSI = iRSI(_Symbol, PERIOD_H1, 14, PRICE_CLOSE);
   handleFastRSI = iRSI(_Symbol, PERIOD_M5, 14, PRICE_CLOSE);

   if(handleSlowRSI == INVALID_HANDLE || handleFastRSI == INVALID_HANDLE) {
      Print("Error creating RSI handles");
      return INIT_FAILED;
   }

   Print("SproutEA started running");
   return INIT_SUCCEEDED;
}

/*
void OnDeinit(const int reason){ //Called when expert stops running
   Print("SproutEA stopped running");
   EventKillTimer();
}
*/


void OnDeinit(const int reason) {
   IndicatorRelease(handleSlowRSI);
   IndicatorRelease(handleFastRSI);
   Print("SproutEA stopped running");
}

// OLD VERSION OF OnTIck() before refactor. Reason of refactor: RSI results in array did not get outputted. EIther not filled in or output method is wrong.
void OnTick(){ //Called every tick. Define mechanism every tick
   static datetime timestamp;
   Print("timestamp: ", timestamp);
   
   datetime time = iTime(_Symbol, PERIOD_CURRENT, 0);
   Print("time: ", time);
   
   if(timestamp != time){
      timestamp = time;
      
      // RSI Strat Here
      //---
          
      static int handleSlowRSI = iRSI(_Symbol, PERIOD_H1, 0, PRICE_CLOSE);
      double slowRSIArray[];
      CopyBuffer(handleSlowRSI, 0, 1, 2, slowRSIArray);
      ArraySetAsSeries(slowRSIArray, true);
      
      static int handleFastRSI = iRSI(_Symbol, PERIOD_M5, 0, PRICE_CLOSE);
      double fastRSIArray[];
      CopyBuffer(handleFastRSI, 0, 1, 2, fastRSIArray);
      ArraySetAsSeries(fastRSIArray, true);
      
      if(fastRSIArray[0] > slowRSIArray[0] && fastRSIArray[1] < slowRSIArray[1]){
         Print("Fast RSI is now > than slow RSI");
         double ask = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
         double sl = ask - 200 * SymbolInfoDouble(_Symbol, SYMBOL_POINT);
         double tp = ask + 100 * SymbolInfoDouble(_Symbol, SYMBOL_POINT);
         trade.Buy(0.01, _Symbol, ask, sl, tp, "This is a buy");
      };
      
      if(fastRSIArray[0] < slowRSIArray[0] && fastRSIArray[1] < slowRSIArray[1]){
         Print("Fast MA is now < than slow MA");
         double bid = SymbolInfoDouble(_Symbol, SYMBOL_BID);
         double sl = bid - 200 * SymbolInfoDouble(_Symbol, SYMBOL_POINT);
         double tp = bid + 100 * SymbolInfoDouble(_Symbol, SYMBOL_POINT);
         trade.Sell(0.01, _Symbol, bid, sl, tp, "This is a sell");
      };
      
      Comment("\nslowRSIArray[0]: ", slowRSIArray[0],
              "\nslowRSIArray[1]: ", slowRSIArray[1],
              "\nfastRSIArray[1]: ", fastRSIArray[1],
              "\nfastRSIArray[1]: ", fastRSIArray[1]
      );
      
      // RSI Strat Here
      /*
      static int handleSlowMA = iMA(_Symbol, PERIOD_CURRENT, 200, 0, MODE_SMA, PRICE_CLOSE);
      double slowMAArray[];
      CopyBuffer(handleSlowMA, 0, 1, 2, slowMAArray);
      ArraySetAsSeries(slowMAArray, true);
      
      static int handleFastMA = iMA(_Symbol, PERIOD_CURRENT, 200, 0, MODE_SMA, PRICE_CLOSE);
      double fastMAArray[];
      CopyBuffer(handleFastMA, 0, 1, 2, fastMAArray);
      ArraySetAsSeries(fastMAArray, true);
      
      if(fastMAArray[0] > slowMAArray[0] && fastMAArray[1] < slowMAArray[1]){
         Print("Fast MA is now > than slow MA");
         double ask = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
         double sl = ask - 200 * SymbolInfoDouble(_Symbol, SYMBOL_POINT);
         double tp = ask + 100 * SymbolInfoDouble(_Symbol, SYMBOL_POINT);
         trade.Buy(0.01, _Symbol, ask, sl, tp, "This is a buy");
      }
      
      if(fastMAArray[0] < slowMAArray[0] && fastMAArray[1] < slowMAArray[1]){
         Print("Fast MA is now < than slow MA");
         double bid = SymbolInfoDouble(_Symbol, SYMBOL_BID);
         double sl = bid - 200 * SymbolInfoDouble(_Symbol, SYMBOL_POINT);
         double tp = bid + 100 * SymbolInfoDouble(_Symbol, SYMBOL_POINT);
         trade.Sell(0.01, _Symbol, bid, sl, tp, "This is a sell");
      }
      
      Comment("\nslowMAArray[0]: ", slowMAArray[0],
              "\nslowMaArray[1]: ", slowMAArray[1],
              "\nfastMaArray[1]: ", fastMAArray[1],
              "\nfastMaArray[1]: ", fastMAArray[1]
      );
      */

   }
}

  
void OnTimer(){
}


void OnTrade(){
}


void OnTradeTransaction(const MqlTradeTransaction& trans,
                        const MqlTradeRequest& request,
                        const MqlTradeResult& result){
}

double OnTester(){
   double ret=0.0;
   return(ret);
}

void OnTesterInit(){
}

void OnTesterPass(){
}

void OnTesterDeinit(){
}


void OnChartEvent(const int32_t id,
                  const long &lparam,
                  const double &dparam,
                  const string &sparam){
}

void OnBookEvent(const string &symbol){
}