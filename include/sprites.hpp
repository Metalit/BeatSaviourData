#include "questui/shared/BeatSaberUI.hpp"

using namespace QuestUI;

const std::string whiteSpriteBase64 = "/9j/4AAQSkZJRgABAQEAwADAAAD/4QAiRXhpZgAATU0AKgAAAAgAAQESAAMAAAABAAEAAAAAAAD/2wBDAAIBAQIBAQ"
"ICAgICAgICAwUDAwMDAwYEBAMFBwYHBwcGBwcICQsJCAgKCAcHCg0KCgsMDAwMBwkODw0MDgsMDAz/2wBDAQICAgMDAwYDAwYMCAcIDAwMDAwMDAwMDAwMDAwMDAwMDA"
"wMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAz/wAARCAABAAEDASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAw"
"UFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eX"
"qDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAA"
"ECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSE"
"lKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+P"
"n6/9oADAMBAAIRAxEAPwD9/KKKKAP/2Q==";

UnityEngine::Sprite* WhiteSprite() {
    static auto whiteSprite = BeatSaberUI::Base64ToSprite(whiteSpriteBase64);
    return whiteSprite;
}

// option a: just use a file. option b: this, I guess
const std::string circleSpriteBase64 = "iVBORw0KGgoAAAANSUhEUgAAAfQAAAH0CAMAAAD8CC+4AAAAAXNSR0IB2cksfwAAAn9QTFRFAAAA/////////////"
"////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////"
"////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////"
"////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////"
"////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////"
"////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////"
"////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////"
"///////////////////////////////////////////////////////////////////Lau0ZAAAANV0Uk5TABQoPVJnfJClusPIztPY3eLo7fL3/AQjQmKBoL/U6f7/I"
"EiGxORKdZ/K9Clknj55s+4GSY7JMHC4HVqX3wJHhMEMmN4VW6EeZavxF2u0+kaZ7HTHT6L2Kn3RXbkui7FErECoO6M2LPNt3FbFP64nA4DvfxGN0h+b4C2pIQ6S+cbLk"
"2xgUzkT47yVbzTMWDEKto9oQRuMpmkistqdXybZ0AVVK1QHr71xJDJOARBqWU3X4QmCC/A1+Iq1HFF3eEUWOsKRCLeFGeUPljNcmhKHpEzN1afnexJ4RAAAAAlwSFlzA"
"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";

UnityEngine::Sprite* CircleSprite() {
    static auto circleSprite = BeatSaberUI::Base64ToSprite(circleSpriteBase64);
    return circleSprite;
}