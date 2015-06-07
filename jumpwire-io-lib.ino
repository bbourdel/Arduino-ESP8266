
//  jumpwire.io client library version 0.0.1
//  (c)2015 IPComSys Co., Ltd.
//  The MIT License (MIT)
//  http://opensource.org/licenses/mit-license.php

//
//  For debugging
//

#define DEBUG_FLG 0 //1: Output debug log
                    //0: Don't output debug log (Fast)

#if DEBUG_FLG //DEBUG_FLG: 1
  
  #include <SoftwareSerial.h>
  SoftwareSerial debugSerial(10, 11); // RX, TX
  char debug_buf[50];
  
  void initDebugSerial(){
    debugSerial.begin(9600);
    int i = 0;
    for(i=0;i<49;i++){
      debug_buf[i] = ' ';
    }
    debug_buf[49] = 0;
  }
  
  void printDebugLog(char *DebugLog){
    debugSerial.println(DebugLog);
  }
  
  void printDebugLogNoln(char *DebugLog){
    debugSerial.print(DebugLog);
  }
  
  void printDebugErr(char *DebugLog){
    debugSerial.println(DebugLog);
    debugSerial.println("log--> ");
    debugSerial.print(debug_buf);
    debugSerial.println(" <--log");
  }
  
  void putDebug_buf(char letter){
    int i = 0;
    for(i=0;i<49;i++){
      debug_buf[i] = debug_buf[i+1];
    }
    debug_buf[49] = letter;
  }

#else  //DEBUG_FLG: 0
  
  void initDebugSerial(){
    //do nothing
  }
  
  void printDebugLog(char *DebugLog){
    //do nothing
  }
  
  void printDebugLogNoln(char *DebugLog){
    //do nothing
  }
  
  void printDebugErr(char *DebugLog){
    //do nothing
  }
  
  void putDebug_buf(char letter){
    //do nothing
  }

#endif

//
//  jumpwire.io settings
//

#define Analog_Unused_pin 0  //Analog PIN for random seed 
#define WS_SERVER         "socket.jumpwire.io"
#define WS_PORT           "80"
#define WS_PATH           "/socket.io/?transport=websocket"
#define PING_INTERVAL     55000 //socket.ioへpingを送る間隔ミリ秒数
#define MAX_MESSAGE_SIZE  100   //処理するメッセージの最大サイズ(MAX254)
#define WAITING_TIMEOUT   10000 //ESPの応答を待つミリ秒数

byte          errorFlg             = 0; //このフラグがたった場合は、接続しなおす
unsigned long pingtimer            = 0; //ping実行用タイマー
char          MessageCheckBuffer[5];    //+IPD,をチェックするためのバッファ
byte          MessageReceivingMode = 0; //受信モード格納用
unsigned int  MessageSize          = 0; //受信パケットサイズ格納用変数
char          MessageBuffer[MAX_MESSAGE_SIZE + 1]; //受信メッセージ格納用バッファ
byte          MessageCursor        = 0;  //何文字目を受信しているかのカーソル

//
//  jumpwire.io API's
//

void jumpwireIoSetup() {
  initDebugSerial();
  printDebugLog("\nstart jumpwire.io");
  randomSeed(analogRead(Analog_Unused_pin)); //for websocket mask
  Serial.begin(your_ESP8266_baud_rate);
  WebSocketConnect();
  pingtimer = millis(); //reset ping timer
}

void jumpwireIoLoop() {
  while (Serial.available()) {
    char a = ProcessReceivedCharacter();//文字列をチェック
  }
  if (millis() > pingtimer + PING_INTERVAL) {  
        //受信待ち状態でタイマーが起動した場合の対策
        MessageReceivingMode = 0;//モード0に戻る
        MessageSize = 0; //メッセージサイズをリセット
        MessageCursor = 0;//メッセージカーソルをリセット   
    WebSocketSendText("2"); //socket.ioのPing (Websocket的にはテキスト)
    pingtimer = millis(); //タスク終了時刻をセット
  }
  if (errorFlg == 1) {
    printDebugErr("errorFlg == 1");
    WebSocketConnect();  //recconect
    pingtimer = millis(); //ループ1週目タスクも完了ししたらPingタイマーリセット
    errorFlg = 0;
  }
}

void Throw(char key, float value){
  String string ="42[\"f\",[\"";
  string += key;
  string += "\",";
  string += value;
  string += "]]\0";
  char buf[string.length()+1];
  string.toCharArray(buf,string.length()+1);
  WebSocketSendText(buf);
  
  string = "";
  string += "Throw key: ";
  string += key;
  string += " value: ";
  string += value;
  char buf2[string.length()+1];
  string.toCharArray(buf2,string.length()+1);
  printDebugLog(buf2);  
}

//
// WebSocketConnect
//

void WebSocketConnect() {
  
  printDebugLog("try ESP8266 AT...");
  Serial.println(F("AT"));
  if(WaitFor("OK\r\n")){
     printDebugLog("  ok: AT");
  }else{
    printDebugErr("  fatal error: check baud rate");
  }
  
  printDebugLog("reset ESP8266...");
  Serial.println(F("AT+RST"));
  if(WaitFor("ready\r\n")){
     printDebugLog("  ok: reseted");
  }else{
    printDebugErr("  error: can't reset ESP8266");
    errorFlg = 0;
  }

  printDebugLog("checking ESP8266 mode...");
  Serial.println(F("AT+CWMODE=1"));
  if(WaitFor("OK\r\n")){
     printDebugLog("  ok: station mode");
  }else{
    printDebugErr("error: not station mode");
  }

  printDebugLog("connecting to WiFi...");
  Serial.print(F("AT+CWJAP=\""));
  Serial.print(your_WiFi_SSID);
  Serial.print(F("\",\""));
  Serial.print(your_WiFi_password);
  Serial.println(F("\""));
  if(WaitFor("OK\r\n")){
     printDebugLog("  ok: connected");
  }else{
    printDebugErr("  error: can't connect WiFi");
  }

  printDebugLog("opening TCP connection...");
  Serial.print(F("AT+CIPSTART=\"TCP\",\""));
  Serial.print(WS_SERVER);
  Serial.print(F("\","));
  Serial.println(WS_PORT);
  if(WaitFor("OK\r\n")){
     printDebugLog("  ok: opened");
  }else{
    printDebugErr("  error: can't open TCP connection");
  }

  char key_base64[] = "dGhlIHNhbXBsZSBub25jZQ=="; //not random at this time

  printDebugLog("opening Websocket connection...");
  Serial.print(F("AT+CIPSEND="));
  Serial.println(StringLength(WS_PATH)+StringLength(jumpwire_io_token)+StringLength(jumpwire_io_project)+StringLength(WS_SERVER) + StringLength(WS_PORT) + 139 +21); //固定部分の文字長は改行含めて149文字
  WaitFor("> "); //wait for prompt

  Serial.print(F("GET "));
  Serial.print(WS_PATH);
  Serial.print(F(" HTTP/1.1\r\nHost: "));
  Serial.print(WS_SERVER);
  Serial.print(F(":"));
  Serial.print(WS_PORT);
  Serial.print(F("\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"));
  Serial.print(F("Sec-WebSocket-Key: "));
  Serial.print(key_base64);
  Serial.print(F("\r\nSec-WebSocket-Version: 13\r\n"));
  Serial.print(F("Cookie: v=1; t="));  //15
  Serial.print(jumpwire_io_token);
  Serial.print(F("; p=")); //4
  Serial.print(jumpwire_io_project);
  Serial.print(F("\r\n")); //2
  Serial.print(F("\r\n"));
  
  if(WaitFor("s3pPLMBiTxaQ9kYGzzhZRbK+xOo=")){
     printDebugLog("  ok: opened");
  }else{
    printDebugErr("  error: can't open Websocket");
  }
  /*
  printDebugLog("Login to jumpwire.io");
  if(WaitFor("authorized\"]")){
     printDebugLog("  ok: welcome!");
  }else{
    printDebugErr("  error: can't login, check token");
  }
  */
  
errorFlg = 0;  //ignore err when connecting
}

//
// socket.io
//

void OnEventReceived(char *str, byte len) {

  //受信したイベントをパースして、コールバック関数を呼び出してあげる

  //まずはイベントが ["f",[" かどうかをチェック
  if ((str[0] == '[')
      && (str[1] == '"')
      && (str[2] == 'f')
      && (str[3] == '"')
      && (str[4] == ',')
      && (str[5] == '[')
      && (str[6] == '"')) {

  //keyをパース    
  char key = str[7];
  
  //文字列か数値かnullかをチェック
  switch(str[10]){
    case '"':   //"なら文字列
      //Serial1.println(F("パース結果：文字列"));//debug
      break;
    case 'n':   //"ならnull
      //Serial1.println(F("パース結果：null"));//debug
      break;
    default:
      if((('0'<=str[10])&&(str[10]<='9'))||(str[10]=='.')){ //ASCIIコードが0〜9の間or'.'
        //Serial1.println(F("パース結果：数値"));//debug
        
        //valueをパース
        float value=0;
        //文字列表記をfloat型に変換
        byte i=10;
        for (i=10;(('0'<=str[i])&&(str[i]<='9'));i++){ //str[i]が数字で有る限りループ
           value = value * 10 + str[i] - 48;  //Asciiコード48を引いて数値に直す
        }
        if(str[i]=='.'){
          byte j=1;
          for (j=1;(('0'<=str[i+j])&&(str[i+j]<='9'));j++){
            value = value + (str[i+j] - 48) / pow(10,j);  //Asciiコード48を引いて数値に直す
          }
        }
        Catch(key, value); //コールバックを呼び出し
        
        String message = "";
        message += "Catch key: ";
        message += key;
        message += " value: ";
        message += value;
        char buf[message.length()+1];
        message.toCharArray(buf,message.length()+1);
        printDebugLog(buf); //debug

      }else{
        //Serial1.println(F("パースエラー")); //それ以外はエラー
      }
  }
  }

}


//
// Receive messeage
//

char ProcessReceivedCharacter() {
  char a = Serial.read(); //文字を読み込み
  putDebug_buf(a);//デバッグバッファに文字格納

  //１文字ずらしてバッファに受信文字を格納
  MessageCheckBuffer[0] = MessageCheckBuffer[1];
  MessageCheckBuffer[1] = MessageCheckBuffer[2];
  MessageCheckBuffer[2] = MessageCheckBuffer[3];
  MessageCheckBuffer[3] = MessageCheckBuffer[4];
  MessageCheckBuffer[4] = a;

  //モード0(メッセージヘッダおよびそのほか受信中)かつ直近の受信5文字が"+IPD,"だったら次の処理を実行
  if ((MessageReceivingMode == 0)
      && (MessageCheckBuffer[0] == '+')
      && (MessageCheckBuffer[1] == 'I')
      && (MessageCheckBuffer[2] == 'P')
      && (MessageCheckBuffer[3] == 'D')
      && (MessageCheckBuffer[4] == ',') ) {
    MessageReceivingMode = 1; //受信モード1に切り替え
    return a; //そのまま次のモードに行かないように結果を返す
  }

  //モード1(メッセージサイズ受信中)の処理
  if (MessageReceivingMode == 1) {
    if ((a >= '0') && (a <= '9')) {
      MessageSize = MessageSize * 10 + a - 48; //十進で一桁ずらして1の位を足す
      //ASCIIコードから48を引いて無理やり整数に直してる
    }
    if (a == ':') {
      MessageReceivingMode = 2;

    }

    //int MessageSize=0;//受信パケットサイズ格納用変数
    return a; //そのまま次のモードに行かないように結果を返す
  }

  //モード2(メッセージ本体受信中)の処理
  if (MessageReceivingMode == 2) {

      MessageBuffer[MessageCursor] = a; //メッセージを格納
      MessageCursor++;//メッセージカーソルを1増やす
      
      //終了処理
      if((MessageCursor == MessageSize) || (MessageCursor == MAX_MESSAGE_SIZE)){
        MessageBuffer[MessageCursor] = '\0'; //最後はnull文字を入れておく
        ProcessMessage(MessageBuffer, MessageCursor);


        MessageReceivingMode = 0;//モード0に戻る
        MessageSize = 0; //メッセージサイズをリセット
        MessageCursor = 0;//メッセージカーソルをリセット
      
      }
    return a;  //そのまま次のモードに行かないように結果を返す
  }

  return a;

}

//メッセージを処理する関数
void ProcessMessage(char *str, byte len) {

  //初めの2文字をチェック

  //B10000001 テキストパケットかつ B0*******マスクなしならテキストフレーム
  //(byte)はstr[i]をバイト型にキャストしている
  if (((byte)str[0] == B10000001) && ((((byte)str[1])&B10000000) == B00000000)) {

    //ペイロードが１文字かつ3ならpongパケット
    if (((byte)str[1] == B00000001) && (str[2] == '3')) {
      //上記にあてはまらず、ペイロードが42から始まっていたらメッセージ
    } else if ((str[2] == '4') && (str[3] == '2')) {
      byte k = 4;
      byte startchar = 4;
      //printDebugLogNoln("/");
      for (k=4;k<len;k++){
        if( ((byte)str[k] == B10000001) || (k == len-1) ){
          
          //printDebugLogNoln("|");

          //startcharからk−１までを送信
          byte j = startchar;
          char EventString[k - startchar + 1]; //最後の+1はnull用
          for (j = startchar; j < k-1; j++) {
            EventString[j - startchar] = str[j];
          }
          EventString[k - startchar] = 0; //最後の1文字はnull
          //printDebugLog(EventString);
          OnEventReceived(EventString, k - startchar);

          startchar = k+4;
        }
      }
      

    }
  } else {
  }
}

//
// Send messeage
//

void WebSocketSendText(char *str) {

  //port文字列の長さを求める TODO:関数化
  byte len = 0;
  while (1) {
    if (str[len] == 0) { //もし配列の中身がnull文字だったら
      break; //ループを抜ける
    } else {
      len++;
    }
  }


  if (len > 125) {
    //125文字以上ならエラーを吐いて何もしない
    while (1) {}
  }


  char WebSocketFrame[6 + len + 1]; //データ格納用のバッファを作成 ヘッダ長6+テキスト長len+末尾のnull文字1(char型で渡すので)
  WebSocketFrame[0] = B10000001; //FIN(1:最終フレーム),RSV1(0),RSV2(0),RSV3(0),opcode(0001:テキストフレーム)
  WebSocketFrame[1] = B10000000; //MASK(1:マスクあり)
  WebSocketFrame[1] = WebSocketFrame[1] + len; //Payload長を足す


  //マスクを用意する
  WebSocketFrame[2] = random(-128, 128); //MASKにランダムな値を入れる
  WebSocketFrame[3] = random(-128, 128); //MASKにランダムな値を入れる
  WebSocketFrame[4] = random(-128, 128); //MASKにランダムな値を入れる
  WebSocketFrame[5] = random(-128, 128); //MASKにランダムな値を入れる

  //ペイロードを用意する
  byte i = 0;
  for (i = 0; i < len; i++) {
    WebSocketFrame[6 + i] = str[i] ^ WebSocketFrame[i % 4 + 2];
    /*変換後のデータの i 番目のオクテット（ "transformed-octet-i", 0 番目が最初）は、
    元データの i 番目のオクテット（ "original-octet-i" ）と,
    マスキングキーの i modulo 4 番目に位置するオクテット（ "masking-key-octet-j" ）との XOR （排他的論理和）である */
  }


  WebSocketFrame[6 + len] = B00000000; //最後の1バイトは文字列の区切り用(つねに0:null文字)Strの最後に入っている
  //出来上がったフレームをTCPで投げる
  TcpSend(WebSocketFrame, 6 + len); //フレームchar*,バイト数

}

//所定の文字列をTCPで送信する関数
void TcpSend(char *str, byte len) {
  Serial.print(F("AT+CIPSEND="));
  Serial.println(len);
  WaitFor("> "); //応答が返ってくるまで待つ
  //64バイト(Serialの送信バッファサイズ)ごとに分けて長い文字列に対応
  int i;
  for (i = 0; i < len; i++) {
    Serial.print(str[i]);
    if ((i % 64) == 63) {
      delay(20); //64バイト送ったら20msお休み
    }
  }
  WaitFor("SEND OK\r\n");
}


//
// utilities
//

//引数で指定した文字列が帰ってくるまで待つ関数
int WaitFor(char *str) {
  unsigned long timeofstart = millis(); //タイムアウト用のタイマーをセット
  byte len = StringLength(str); //strはポインタ
  char buf[len]; //バッファを確保
  byte mode = 0; //0：不一致 1：一致
  while (1) { //所定の条件に当てはまるまでループし続ける
    if (Serial.available()) { //文字を受信した場合
      char a = Serial.read(); //文字を読み込み
      putDebug_buf(a);//デバッグバッファに文字格納
      //受信した文字をバッファの最後に入れる
      byte i;
      for (i = 0; i < len; i++) {
        buf[i] = buf[i + 1]; //バッファを一文字前にずらす
      }
      buf[len - 1] = a; //バッファの最後にaを格納
      //所定の文字列に一致しているか判定
      mode = 1; //一旦一致したようにフラグ立て
      for (i = 0; i < len; i++) {
        if (buf[i] != str[i]) { //i文字目同士が一致していない場合
          mode = 0; //１文字でも不一致があれば0に戻す
        }
      }
      if (mode == 1) {
      return 1; //一致したのでループを抜ける
      }
    }
    //タイムアウトを設定
    if ((millis() - timeofstart) > WAITING_TIMEOUT) { //タイムアウト  
      printDebugLog("timeout: when waiting for");
      printDebugLogNoln("-->");
      printDebugLogNoln(str);
      printDebugLog("<--");
      errorFlg = 1; //エラーフラグを立てる
      //timer = millis(); //接続が完了したらタイマーリセット
      pingtimer = millis(); //接続が完了したらPingタイマーリセット
      return 0;
    }
  }
}

byte StringLength(char *str) {
  byte len = 0;
  while (1) {
    if (str[len] == 0) {
      break;
    } else {
      len++;
    }
  }
  return len;
}
