#pragma once
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <array>
#include <iostream>
#include <sstream>
#include <optional>
#include <cstdlib>
#include <ctime>

#define GRIDSIZE 35
#define STAGE_WIDTH 10
#define STAGE_HEIGHT 20
#define FIELD_WIDTH 10
#define FIELD_HEIGHT 24 // 实际场地高度，比可见舞台多4行用于缓冲

enum PLAYROLE
{
    roleNONE,
    rolePLAYER1,
    rolePLAYER2
};

class Tetris
{
public:
    Tetris();
    ~Tetris();

    void Initial(const std::array<sf::Texture, 8> *tex, sf::RenderWindow *w, PLAYROLE r);
    void Input(const std::optional<sf::Event> &event);
    void Logic();
    void Draw();
    sf::Vector2i getCorner() const;

    // 供Game类调用的公共接口
    bool isGameOver;
    float timer;
    float delay;
    int score;
    int role;

    // 对战相关：添加垃圾行
    void addGarbageRow(int lines);

private:
    bool hitTest();
    void clearLines();
    void spawnNewPiece();
    void fixCurrentPiece();

    // 游戏数据
    int Field[FIELD_HEIGHT][FIELD_WIDTH];
    sf::Vector2i currentSquare[4];
    sf::Vector2i tempSquare[4];
    int colorNum; // 当前方块的纹理索引
    int nextColorNum;
    int nextShapeIndex;
    int dx; // 水平移动偏移量
    bool isRotate;

    const std::array<sf::Texture, 8> *tTiles; // 7种方块纹理，索引1~7
    sf::RenderWindow *window;
    sf::Texture emptyTexture;
    sf::Sprite sTiles;
    sf::Vector2i mCornPoint; // 绘制偏移量

    // 形状表（基于2x4矩阵编码，数字0-7）
    int Figures[7][4];
    void initFigures();
};