#ifndef UI_RENDERER_H
#define UI_RENDERER_H

#include <string>

// Draw upper status bar with mode title, battery icon, and sticky mode indicators
void drawStatusBar(const char* title);

// Draw REPL screen
void drawCalc();

// Draw highlighted expression with rainbow brackets & cursor
void drawHighlightedExpression(const std::string& expr);

// Draw interactive manager screens
void drawVars();
void drawBinds();
void drawFormulas();
void drawScripts();
void drawParams();
void drawConsts();

// Draw full-screen help summary view
void drawHelpView();

// Draw Fn+H help modal overlay
void drawHelpPopup();

#endif // UI_RENDERER_H
