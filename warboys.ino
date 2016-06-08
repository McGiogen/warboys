/*
  Breakout
  Copyright (C) 2011 Sebastian Goscik
  All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
*/

#include "Arduboy.h"
#include "breakout_bitmaps.h"

Arduboy arduboy;

#define SCREEN_MAX_X 127
#define SCREEN_MAX_Y 63

#define COLUMNS 9          //Columns of bricks
#define ROWS 3              //Rows of bricks
#define BLOCK_LENGTH 3      //Rows of bricks
#define NUM_PLAYERS 4       //Number of players

int dx = -1;        //Initial movement of ball
int dy = -1;        //Initial movement of ball
int xb;           //Balls starting possition
int yb;           //Balls starting possition
boolean released;     //If the ball has been released by the player
boolean paused = false;   //If the game has been paused
int paddle_fixed_distance = (COLUMNS + (ROWS * 2)) / 2 * BLOCK_LENGTH + 2;  // (lineBlocks/2) * block_length + span   //Fixed distance of the paddle from borders
byte paddle_movement;       //X position of paddle
boolean isHit[NUM_PLAYERS][ROWS][COLUMNS + ROWS * 2]; //Array of if bricks are hit or not
boolean bounced = false; //Used to fix double bounce glitch
byte lives = 3;       //Amount of lives
byte level = 1;       //Current level
unsigned int score = 0; //Score for the game
unsigned int brickCount;  //Amount of bricks hit
byte pad, pad2, pad3;   //Button press buffer used to stop pause repeating
byte oldpad, oldpad2, oldpad3;
char text[16];      //General string buffer
boolean start = false;  //If in menu or in game
boolean initialDraw = false; //If the inital draw has happened
char initials[3];     //Initials used in high score

//Ball Bounds used in collision detection
byte leftBall;
byte rightBall;
byte topBall;
byte bottomBall;

//Brick Bounds used in collision detection
byte leftBrick;
byte rightBrick;
byte topBrick;
byte bottomBrick;

byte tick;

struct Coordinates {
  int x;
  int y;
} block;

struct Paddle {
  Coordinates v1;
  Coordinates v2;
  Coordinates v3;
};

#include "pins_arduino.h" // Arduino pre-1.0 needs this

void intro()
{
  for (int i = -8; i < 28; i = i + 2)
  {
    arduboy.clear();
    arduboy.setCursor(46, i);
    arduboy.print("ARDUBOY");
    arduboy.display();
  }

  arduboy.tunes.tone(987, 160);
  delay(160);
  arduboy.tunes.tone(1318, 400);
  delay(2000);
}

void movePaddle()
{
  //Move right
  byte maxMovement = paddle_fixed_distance * 2 - 1;
  if (paddle_movement < maxMovement)
  {
    if (arduboy.pressed(RIGHT_BUTTON))
    {
      paddle_movement += 2;
      paddle_movement = paddle_movement > maxMovement ? maxMovement : paddle_movement;
    }
  }

  //Move left
  byte minMovement = 1;
  if (paddle_movement > minMovement)
  {
    if (arduboy.pressed(LEFT_BUTTON))
    {
      paddle_movement -= 2;
      paddle_movement = paddle_movement < minMovement ? minMovement : paddle_movement;
    }
  }
}

void moveBall()
{
  tick++;
  if (released)
  {
    //Move ball
    if (abs(dx) == 2) {
      xb += dx / 2;
      // 2x speed is really 1.5 speed
      if (tick % 2 == 0)
        xb += dx / 2;
    } else {
      xb += dx;
    }
    yb = yb + dy;

    //Set bounds
    leftBall = xb;
    rightBall = xb + 2;
    topBall = yb;
    bottomBall = yb + 2;

    //Bounce off top edge
    if (yb <= 0)
    {
      yb = 2;
      dy = -dy;
      arduboy.tunes.tone(523, 250);
    }

    //Bounce off bottom edge
    if (yb >= 62)
    {
      yb = 60;
      dy = -dy;
      arduboy.tunes.tone(523, 250);
    }
    //Lose a life if bottom edge hit
//    if (yb >= 64)
//    {
//      arduboy.drawRect(paddle_movement, 63, 11, 1, 0);
//      paddle_movement = 54;
//      yb = 60;
//      released = false;
//      lives--;
//      drawLives();
//      arduboy.tunes.tone(175, 250);
//      if (random(0, 2) == 0)
//      {
//        dx = 1;
//      }
//      else
//      {
//        dx = -1;
//      }
//    }

    //Bounce off left side
    if (xb <= 0)
    {
      xb = 2;
      dx = -dx;
      arduboy.tunes.tone(523, 250);
    }

    //Bounce off right side
    if (xb >= WIDTH - 2)
    {
      xb = WIDTH - 4;
      dx = -dx;
      arduboy.tunes.tone(523, 250);
    }

    //Bounce off paddle
    byte player = 0;
    Paddle paddle = getPaddle(paddle_movement, 0);
    if (xb + 1 >= paddle.v1.x && xb <= paddle.v2.x + 5 && yb + 2 >= paddle.v1.y && yb <= paddle.v2.y+1)
    {
      dy = -dy;
      dx = ((xb - (paddle_movement + 6)) / 3); //Applies spin on the ball
      // prevent straight bounce
      if (dx == 0) {
        dx = (random(0, 2) == 1) ? 1 : -1;
      }
      arduboy.tunes.tone(200, 250);
    }

    //Bounce off Bricks
    Coordinates block;
    for (byte player = 0; player < NUM_PLAYERS; player++) {
      for (byte lineIndex = 0; lineIndex < ROWS; lineIndex++)
      {
      byte lineBlocks = COLUMNS + lineIndex * 2;
      for (byte blockIndex = 0; blockIndex < lineBlocks; blockIndex++)
      {
        if (!isHit[player][lineIndex][blockIndex])
        {
          //Sets Brick bounds
          block = getBlock(lineIndex, blockIndex, player);

          leftBrick = block.x;
          rightBrick = block.x + BLOCK_LENGTH;
          topBrick = block.y;
          bottomBrick = block.y + BLOCK_LENGTH;

          //If A collison has occured
          if (topBall <= bottomBrick && bottomBall >= topBrick &&
              leftBall <= rightBrick && rightBall >= leftBrick)
          {
            Score();
            brickCount++;
            isHit[player][lineIndex][blockIndex] = true;
            arduboy.drawRect(block.x, block.y, BLOCK_LENGTH, BLOCK_LENGTH, 0);

            //Vertical collision
            if (bottomBall > bottomBrick || topBall < topBrick)
            {
              //Only bounce once each ball move
              if (!bounced)
              {
                dy = - dy;
                yb += dy;
                bounced = true;
                arduboy.tunes.tone(261, 250);
              }
            }

            //Hoizontal collision
            if (leftBall < leftBrick || rightBall > rightBrick)
            {
              //Only bounce once brick each ball move
              if (!bounced)
              {
                dx = - dx;
                xb += dx;
                bounced = true;
                arduboy.tunes.tone(261, 250);
              }
            }
          }
        }
      }
    }
    }
    //Reset Bounce
    bounced = false;
  }
  else
  {
    //Ball follows paddle
    byte player = 0;
    Paddle paddle = getPaddle(paddle_movement, player);
    xb = paddle.v3.x;
    yb = paddle.v3.y;
    //Adding some distance from paddle
    if (xb > yb) {
      xb += 2;
    } else {
      yb += 2;
    }

    //Release ball if FIRE pressed
    pad3 = arduboy.pressed(A_BUTTON) || arduboy.pressed(B_BUTTON);
    if (pad3 == 1 && oldpad3 == 0)
    {
      released = true;

      //Apply random direction to ball on release
      if (random(0, 2) == 0)
      {
        dx = 1;
      }
      else
      {
        dx = -1;
      }
      //Makes sure the ball heads downwards
      dy = +1;
    }
    oldpad3 = pad3;
  }
}

void drawBall()
{
  // arduboy.setCursor(0,0);
  // arduboy.print(arduboy.cpuLoad());
  // arduboy.print("  ");
  arduboy.drawPixel(xb,   yb,   0);
  arduboy.drawPixel(xb + 1, yb,   0);
  arduboy.drawPixel(xb,   yb + 1, 0);
  arduboy.drawPixel(xb + 1, yb + 1, 0);

  moveBall();

  arduboy.drawPixel(xb,   yb,   1);
  arduboy.drawPixel(xb + 1, yb,   1);
  arduboy.drawPixel(xb,   yb + 1, 1);
  arduboy.drawPixel(xb + 1, yb + 1, 1);
}

void drawPaddle()
{
  Paddle paddle;
  byte player = 0;
  paddle = getPaddle(paddle_movement, player);
  arduboy.fillTriangle(paddle.v1.x, paddle.v1.y, paddle.v2.x, paddle.v2.y, paddle.v3.x, paddle.v3.y, 0);
//  arduboy.drawRect(paddle_movement, 63, 11, 1, 0);
  movePaddle();
//  arduboy.drawRect(paddle_movement, 63, 11, 1, 1);
  paddle = getPaddle(paddle_movement, player);
  arduboy.fillTriangle(paddle.v1.x, paddle.v1.y, paddle.v2.x, paddle.v2.y, paddle.v3.x, paddle.v3.y, 1);
}

void drawLives()
{
  sprintf(text, "LIVES:%u", lives);
  arduboy.setCursor(0, 90);
  arduboy.print(text);
}

void drawGameOver()
{
  arduboy.drawPixel(xb,   yb,   0);
  arduboy.drawPixel(xb + 1, yb,   0);
  arduboy.drawPixel(xb,   yb + 1, 0);
  arduboy.drawPixel(xb + 1, yb + 1, 0);
  arduboy.setCursor(52, 42);
  arduboy.print( "Game");
  arduboy.setCursor(52, 54);
  arduboy.print("Over");
  arduboy.display();
  delay(4000);
}

void pause()
{
  paused = true;
  //Draw pause to the screen
  arduboy.setCursor(52, 45);
  arduboy.print("PAUSE");
  arduboy.display();
  while (paused)
  {
    delay(150);
    //Unpause if FIRE is pressed
    pad2 = arduboy.pressed(A_BUTTON) || arduboy.pressed(B_BUTTON);
    if (pad2 > 1 && oldpad2 == 0 && released)
    {
      arduboy.fillRect(52, 45, 30, 11, 0);

      paused = false;
    }
    oldpad2 = pad2;
  }
}

void Score()
{
  score += (level * 10);
  sprintf(text, "SCORE:%u", score);
  arduboy.setCursor(80, 90);
  arduboy.print(text);
}

void newLevel() {
  Paddle paddle;
  byte player = 0;
  
  //Undraw paddle
  paddle = getPaddle(paddle_movement, player);
  arduboy.fillTriangle(paddle.v1.x, paddle.v1.y, paddle.v2.x, paddle.v2.y, paddle.v3.x, paddle.v3.y, 0);

  //Undraw ball
  arduboy.drawPixel(xb,   yb,   0);
  arduboy.drawPixel(xb + 1, yb,   0);
  arduboy.drawPixel(xb,   yb + 1, 0);
  arduboy.drawPixel(xb + 1, yb + 1, 0);

  //Alter various variables to reset the game
  paddle_movement = 0; //paddle_fixed_distance;
  yb = 60;
  brickCount = 0;
  released = false;

  //Draws new bricks and resets their values
  Coordinates block;
  for (byte player = 0; player < NUM_PLAYERS; player++) {
    for (byte lineIndex = 0; lineIndex < ROWS; lineIndex++) {
      byte lineBlocks = COLUMNS + lineIndex * 2;
      for (byte blockIndex = 0; blockIndex < lineBlocks; blockIndex++)
      {
        isHit[player][lineIndex][blockIndex] = false;
        block = getBlock(lineIndex, blockIndex, player);
        arduboy.drawRect(block.x, block.y, BLOCK_LENGTH, BLOCK_LENGTH, 1);
      }
    }
  }

  //Draws the initial lives
  drawLives();

  //Draws the initial score
  sprintf(text, "SCORE:%u", score);
  arduboy.setCursor(80, 90);
  arduboy.print(text);
}

//Used to delay images while reading button input
boolean pollFireButton(int n)
{
  for (int i = 0; i < n; i++)
  {
    delay(15);
    pad = arduboy.pressed(A_BUTTON) || arduboy.pressed(B_BUTTON);
    if (pad == 1 && oldpad == 0)
    {
      oldpad3 = 1; //Forces pad loop 3 to run once
      return true;
    }
    oldpad = pad;
  }
  return false;
}

//Function by nootropic design to display highscores
boolean displayHighScores(byte file)
{
  byte y = 10;
  byte x = 24;
  // Each block of EEPROM has 10 high scores, and each high score entry
  // is 5 bytes long:  3 bytes for initials and two bytes for score.
  int address = file * 10 * 5;
  byte hi, lo;
  arduboy.clear();
  arduboy.setCursor(32, 0);
  arduboy.print("HIGH SCORES");
  arduboy.display();

  for (int i = 0; i < 10; i++)
  {
    sprintf(text, "%2d", i + 1);
    arduboy.setCursor(x, y + (i * 8));
    arduboy.print( text);
    arduboy.display();
    hi = EEPROM.read(address + (5 * i));
    lo = EEPROM.read(address + (5 * i) + 1);

    if ((hi == 0xFF) && (lo == 0xFF))
    {
      score = 0;
    }
    else
    {
      score = (hi << 8) | lo;
    }

    initials[0] = (char)EEPROM.read(address + (5 * i) + 2);
    initials[1] = (char)EEPROM.read(address + (5 * i) + 3);
    initials[2] = (char)EEPROM.read(address + (5 * i) + 4);

    if (score > 0)
    {
      sprintf(text, "%c%c%c %u", initials[0], initials[1], initials[2], score);
      arduboy.setCursor(x + 24, y + (i * 8));
      arduboy.print(text);
      arduboy.display();
    }
  }
  if (pollFireButton(300))
  {
    return true;
  }
  return false;
  arduboy.display();
}

boolean titleScreen()
{
  //Clears the screen
  arduboy.clear();
  arduboy.setCursor(16, 22);
  arduboy.setTextSize(2);
  arduboy.print("ARAKNOID");
  arduboy.setTextSize(1);
  arduboy.display();
  if (pollFireButton(25))
  {
    return true;
  }

  //Flash "Press FIRE" 5 times
  for (byte i = 0; i < 5; i++)
  {
    //Draws "Press FIRE"
    //arduboy.bitmap(31, 53, fire);  arduboy.display();
    arduboy.setCursor(31, 53);
    arduboy.print("PRESS FIRE!");
    arduboy.display();

    if (pollFireButton(50))
    {
      return true;
    }
    //Removes "Press FIRE"
    arduboy.clear();
    arduboy.setCursor(16, 22);
    arduboy.setTextSize(2);
    arduboy.print("ARAKNOID");
    arduboy.setTextSize(1);
    arduboy.display();

    arduboy.display();
    if (pollFireButton(25))
    {
      return true;
    }
  }

  return false;
}

//Function by nootropic design to add high scores
void enterInitials()
{
  char index = 0;

  arduboy.clear();

  initials[0] = ' ';
  initials[1] = ' ';
  initials[2] = ' ';

  while (true)
  {
    arduboy.display();
    arduboy.clear();

    arduboy.setCursor(16, 0);
    arduboy.print("HIGH SCORE");
    sprintf(text, "%u", score);
    arduboy.setCursor(88, 0);
    arduboy.print(text);
    arduboy.setCursor(56, 20);
    arduboy.print(initials[0]);
    arduboy.setCursor(64, 20);
    arduboy.print(initials[1]);
    arduboy.setCursor(72, 20);
    arduboy.print(initials[2]);
    for (byte i = 0; i < 3; i++)
    {
      arduboy.drawLine(56 + (i * 8), 27, 56 + (i * 8) + 6, 27, 1);
    }
    arduboy.drawLine(56, 28, 88, 28, 0);
    arduboy.drawLine(56 + (index * 8), 28, 56 + (index * 8) + 6, 28, 1);
    delay(150);

    if (arduboy.pressed(LEFT_BUTTON) || arduboy.pressed(B_BUTTON))
    {
      index--;
      if (index < 0)
      {
        index = 0;
      } else
      {
        arduboy.tunes.tone(1046, 250);
      }
    }

    if (arduboy.pressed(RIGHT_BUTTON))
    {
      index++;
      if (index > 2)
      {
        index = 2;
      }  else {
        arduboy.tunes.tone(1046, 250);
      }
    }

    if (arduboy.pressed(DOWN_BUTTON))
    {
      initials[index]++;
      arduboy.tunes.tone(523, 250);
      // A-Z 0-9 :-? !-/ ' '
      if (initials[index] == '0')
      {
        initials[index] = ' ';
      }
      if (initials[index] == '!')
      {
        initials[index] = 'A';
      }
      if (initials[index] == '[')
      {
        initials[index] = '0';
      }
      if (initials[index] == '@')
      {
        initials[index] = '!';
      }
    }

    if (arduboy.pressed(UP_BUTTON))
    {
      initials[index]--;
      arduboy.tunes.tone(523, 250);
      if (initials[index] == ' ') {
        initials[index] = '?';
      }
      if (initials[index] == '/') {
        initials[index] = 'Z';
      }
      if (initials[index] == 31) {
        initials[index] = '/';
      }
      if (initials[index] == '@') {
        initials[index] = ' ';
      }
    }

    if (arduboy.pressed(A_BUTTON))
    {
      if (index < 2)
      {
        index++;
        arduboy.tunes.tone(1046, 250);
      } else {
        arduboy.tunes.tone(1046, 250);
        return;
      }
    }
  }

}

void enterHighScore(byte file)
{
  // Each block of EEPROM has 10 high scores, and each high score entry
  // is 5 bytes long:  3 bytes for initials and two bytes for score.
  int address = file * 10 * 5;
  byte hi, lo;
  char tmpInitials[3];
  unsigned int tmpScore = 0;

  // High score processing
  for (byte i = 0; i < 10; i++)
  {
    hi = EEPROM.read(address + (5 * i));
    lo = EEPROM.read(address + (5 * i) + 1);
    if ((hi == 0xFF) && (lo == 0xFF))
    {
      // The values are uninitialized, so treat this entry
      // as a score of 0.
      tmpScore = 0;
    } else
    {
      tmpScore = (hi << 8) | lo;
    }
    if (score > tmpScore)
    {
      enterInitials();
      for (byte j = i; j < 10; j++)
      {
        hi = EEPROM.read(address + (5 * j));
        lo = EEPROM.read(address + (5 * j) + 1);

        if ((hi == 0xFF) && (lo == 0xFF))
        {
          tmpScore = 0;
        }
        else
        {
          tmpScore = (hi << 8) | lo;
        }

        tmpInitials[0] = (char)EEPROM.read(address + (5 * j) + 2);
        tmpInitials[1] = (char)EEPROM.read(address + (5 * j) + 3);
        tmpInitials[2] = (char)EEPROM.read(address + (5 * j) + 4);

        // write score and initials to current slot
        EEPROM.write(address + (5 * j), ((score >> 8) & 0xFF));
        EEPROM.write(address + (5 * j) + 1, (score & 0xFF));
        EEPROM.write(address + (5 * j) + 2, initials[0]);
        EEPROM.write(address + (5 * j) + 3, initials[1]);
        EEPROM.write(address + (5 * j) + 4, initials[2]);

        // tmpScore and tmpInitials now hold what we want to
        //write in the next slot.
        score = tmpScore;
        initials[0] = tmpInitials[0];
        initials[1] = tmpInitials[1];
        initials[2] = tmpInitials[2];
      }

      score = 0;
      initials[0] = ' ';
      initials[1] = ' ';
      initials[2] = ' ';

      return;
    }
  }
}


void setup()
{
  arduboy.begin();
  arduboy.setFrameRate(60);
  arduboy.print("Hello World!");
  arduboy.display();
  intro();
}


void loop()
{
  // pause render until it's time for the next frame
  if (!(arduboy.nextFrame()))
    return;

  //Title screen loop switches from title screen
  //and high scores until FIRE is pressed
  while (!start)
  {
    start = titleScreen();
    if (!start)
    {
      start = displayHighScores(2);
    }
  }

  //Initial level draw
  if (!initialDraw)
  {
    //Clears the screen
    arduboy.display();
    arduboy.clear();
    //Selects Font
    //Draws the new level
    newLevel();
    initialDraw = true;
  }

  if (lives > 0)
  {
    drawPaddle();

    //Pause game if FIRE pressed
    pad = arduboy.pressed(A_BUTTON) || arduboy.pressed(B_BUTTON);

    if (pad > 1 && oldpad == 0 && released)
    {
      oldpad2 = 0; //Forces pad loop 2 to run once
      pause();
    }

    oldpad = pad;
    drawBall();

    if (brickCount == ROWS * COLUMNS)
    {
      level++;
      newLevel();
    }
  }
  else
  {
    drawGameOver();
    if (score > 0)
    {
      enterHighScore(2);
    }

    arduboy.clear();
    initialDraw = false;
    start = false;
    lives = 3;
    score = 0;
    newLevel();
  }

  arduboy.display();
}

Coordinates getBlock(byte lineIndex, byte blockIndex, byte player) {
  boolean reverse_x = player % 2 == 1;   //players 1 and 3
  boolean reverse_y = player >= 2;       //players 2 and 3
  
  byte lineBlocks = COLUMNS + lineIndex * 2; 
  if (blockIndex <= lineBlocks / 2 - 1) {
    //Draw vertical blocks
    block.x = lineBlocks / 2 * BLOCK_LENGTH;
    block.y = BLOCK_LENGTH * blockIndex;
  } else {
    //Draw horizontal blocks
    block.x = BLOCK_LENGTH * blockIndex - (lineBlocks / 2 * BLOCK_LENGTH);
    block.y = (lineBlocks / 2 * BLOCK_LENGTH);
  }

  if (reverse_x) {
    block.x = SCREEN_MAX_X - block.x - BLOCK_LENGTH;
  }
  if (reverse_y) {
    block.y = SCREEN_MAX_Y - block.y - BLOCK_LENGTH;
  }
  
  return block;
}

Paddle getPaddle(byte paddle_movement, byte player) {
  Paddle paddleCoordinates;
  byte movement;
  boolean isMovingHorizontally; // True if paddle is directed at horizontal
  
  if (player == 0) {
    movement = paddle_movement % paddle_fixed_distance;
    isMovingHorizontally = (paddle_movement / paddle_fixed_distance == 0);
    
    if (isMovingHorizontally) {
      paddleCoordinates.v1.x = movement;
      paddleCoordinates.v1.y = paddle_fixed_distance;
      paddleCoordinates.v2.x = movement + 4;
      paddleCoordinates.v2.y = paddle_fixed_distance;
      paddleCoordinates.v3.x = movement + 2;
      paddleCoordinates.v3.y = paddle_fixed_distance + 3;
    } else {
      movement = paddle_fixed_distance - movement;
      paddleCoordinates.v1.y = movement;
      paddleCoordinates.v1.x = paddle_fixed_distance;
      paddleCoordinates.v2.y = movement + 4;
      paddleCoordinates.v2.x = paddle_fixed_distance;
      paddleCoordinates.v3.y = movement + 2;
      paddleCoordinates.v3.x = paddle_fixed_distance + 3;
    }
  }

  return paddleCoordinates;
}



