#pragma once
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <array>
#include <iostream>
#include <sstream>
#include "Tetris.h"

class TetrisGame
{
public:
    explicit TetrisGame(int requestedPlayers);
    ~TetrisGame();
    void gameRun();

private:
    enum class ResultPhase
    {
        Playing,
        EliminationNotice,
        VictoryNotice,
        Finished
    };

    void gameInitial();
    void LoadMediaData();
    void gameInput();
    void gameLogic();
    void gameDraw();
    void startResultSequence(int loserPlayer, int winnerPlayer);
    void drawResultOverlay();
    void drawModeHint();

    sf::RenderWindow window;
    Tetris player1, player2;
    bool isGameOver;
    bool isGameQuit;
    ResultPhase resultPhase;
    int resultLoserPlayer;
    int resultWinnerPlayer;
    sf::Clock resultClock;
    sf::Font uiFont;
    bool hasUiFont;

    sf::Texture tBackground;
    std::array<sf::Texture, 8> tTiles;
    sf::Texture emptyTexture;
    sf::Sprite sBackground;

    int window_width, window_height;
    int imgBGno, imgSkinNo;
    int requestedPlayerCount;
    std::string playerModeHint;
    sf::Clock gameClock;
};