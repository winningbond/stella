//============================================================================
//
//   SSSS    tt          lll  lll
//  SS  SS   tt           ll   ll
//  SS     tttttt  eeee   ll   ll   aaaa
//   SSSS    tt   ee  ee  ll   ll      aa
//      SS   tt   eeeeee  ll   ll   aaaaa  --  "An Atari 2600 VCS Emulator"
//  SS  SS   tt   ee      ll   ll  aa  aa
//   SSSS     ttt  eeeee llll llll  aaaaa
//
// Copyright (c) 1995-2019 by Bradford W. Mott, Stephen Anthony
// and the Stella Team
//
// See the file "License.txt" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//============================================================================

#include <sstream>

#include "bspf.hxx"

#include "OSystem.hxx"
#include "GuiObject.hxx"
#include "FrameBuffer.hxx"
#include "EventHandler.hxx"
#include "Event.hxx"
#include "OSystem.hxx"
#include "EditTextWidget.hxx"
#include "StringListWidget.hxx"
#include "Widget.hxx"
#include "Font.hxx"
#include "ComboDialog.hxx"
#include "Variant.hxx"
#include "EventMappingWidget.hxx"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
EventMappingWidget::EventMappingWidget(GuiObject* boss, const GUI::Font& font,
                                       int x, int y, int w, int h,
                                       const StringList& actions, EventMode mode)
  : Widget(boss, font, x, y, w, h),
    CommandSender(boss),
    myComboDialog(nullptr),
    myEventMode(mode),
    myActionSelected(-1),
    myRemapStatus(false),
    myLastStick(0),
    myLastAxis(0),
    myLastHat(0),
    myLastValue(0),
    myFirstTime(true)
{
  const int fontHeight   = font.getFontHeight(),
            lineHeight   = font.getLineHeight(),
            buttonWidth  = font.getStringWidth("Defaults") + 10,
            buttonHeight = font.getLineHeight() + 4;
  const int HBORDER = 8;
  const int VBORDER = 8;
  int xpos = HBORDER, ypos = VBORDER;

  myActionsList = new StringListWidget(boss, font, xpos, ypos,
                                       _w - buttonWidth - HBORDER * 2 - 8, _h - 3*lineHeight - VBORDER);
  myActionsList->setTarget(this);
  myActionsList->setEditable(false);
  myActionsList->setList(actions);
  addFocusWidget(myActionsList);

  // Add remap, erase, cancel and default buttons
  xpos = _w - HBORDER - buttonWidth;
  myMapButton = new ButtonWidget(boss, font, xpos, ypos,
                                 buttonWidth, buttonHeight,
                                 "Map" + ELLIPSIS, kStartMapCmd);
  myMapButton->setTarget(this);
  addFocusWidget(myMapButton);

  ypos += lineHeight + 10;
  myCancelMapButton = new ButtonWidget(boss, font, xpos, ypos,
                                       buttonWidth, buttonHeight,
                                       "Cancel", kStopMapCmd);
  myCancelMapButton->setTarget(this);
  myCancelMapButton->clearFlags(Widget::FLAG_ENABLED);
  addFocusWidget(myCancelMapButton);

  ypos += lineHeight + 20;
  myEraseButton = new ButtonWidget(boss, font, xpos, ypos,
                                   buttonWidth, buttonHeight,
                                   "Erase", kEraseCmd);
  myEraseButton->setTarget(this);
  addFocusWidget(myEraseButton);

  ypos += lineHeight + 10;
  myResetButton = new ButtonWidget(boss, font, xpos, ypos,
                                   buttonWidth, buttonHeight,
                                   "Reset", kResetCmd);
  myResetButton->setTarget(this);
  addFocusWidget(myResetButton);

  if(mode == kEmulationMode)
  {
    ypos += lineHeight + 20;
    myComboButton = new ButtonWidget(boss, font, xpos, ypos,
                                     buttonWidth, buttonHeight,
                                     "Combo" + ELLIPSIS, kComboCmd);
    myComboButton->setTarget(this);
    addFocusWidget(myComboButton);

    VariantList combolist = instance().eventHandler().getComboList(mode);
    myComboDialog = new ComboDialog(boss, font, combolist);
  }
  else
    myComboButton = nullptr;

  // Show message for currently selected event
  xpos = HBORDER;  ypos = VBORDER + myActionsList->getHeight() + 8;
  StaticTextWidget* t;
  t = new StaticTextWidget(boss, font, xpos, ypos+2, font.getStringWidth("Action"),
                           fontHeight, "Action", TextAlign::Left);

  myKeyMapping = new EditTextWidget(boss, font, xpos + t->getWidth() + 8, ypos,
                                    _w - xpos - t->getWidth() - 8 - HBORDER, lineHeight, "");
  myKeyMapping->setEditable(false, true);
  myKeyMapping->clearFlags(Widget::FLAG_RETAIN_FOCUS);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void EventMappingWidget::loadConfig()
{
  if(myFirstTime)
  {
    myActionsList->setSelected(0);
    myFirstTime = false;
  }

  // Make sure remapping is turned off, just in case the user didn't properly
  // exit last time
  if(myRemapStatus)
    stopRemapping();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void EventMappingWidget::saveConfig()
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void EventMappingWidget::setDefaults()
{
  instance().eventHandler().setDefaultMapping(Event::NoType, myEventMode);
  drawKeyMapping();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void EventMappingWidget::startRemapping()
{
  if(myActionSelected < 0 || myRemapStatus)
    return;

  // Set the flags for the next event that arrives
  myRemapStatus = true;

  // Reset all previous events for determining correct axis/hat values
  myLastStick = myLastAxis = myLastHat = myLastValue = -1;

  // Reset the previously aggregated key mappings
  myMod = myKey = 0;

  // Disable all other widgets while in remap mode, except enable 'Cancel'
  enableButtons(false);

  // And show a message indicating which key is being remapped
  ostringstream buf;
  buf << "Select action for '"
      << instance().eventHandler().actionAtIndex(myActionSelected, myEventMode)
      << "' event";
  myKeyMapping->setTextColor(kTextColorEm);
  myKeyMapping->setText(buf.str());

  // Make sure that this widget receives all raw data, before any
  // pre-processing occurs
  myActionsList->setFlags(Widget::FLAG_WANTS_RAWDATA);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void EventMappingWidget::eraseRemapping()
{
  if(myActionSelected < 0)
    return;

  Event::Type event =
    instance().eventHandler().eventAtIndex(myActionSelected, myEventMode);
  instance().eventHandler().eraseMapping(event, myEventMode);

  drawKeyMapping();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void EventMappingWidget::resetRemapping()
{
  if(myActionSelected < 0)
    return;

  Event::Type event =
    instance().eventHandler().eventAtIndex(myActionSelected, myEventMode);
  instance().eventHandler().setDefaultMapping(event, myEventMode);

  drawKeyMapping();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void EventMappingWidget::stopRemapping()
{
  // Turn off remap mode
  myRemapStatus = false;

  // Reset all previous events for determining correct axis/hat values
  myLastStick = myLastAxis = myLastHat = myLastValue = -1;

  // And re-enable all the widgets
  enableButtons(true);

  // Make sure the list widget is in a known state
  drawKeyMapping();

  // Widget is now free to process events normally
  myActionsList->clearFlags(Widget::FLAG_WANTS_RAWDATA);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void EventMappingWidget::drawKeyMapping()
{
  if(myActionSelected >= 0)
  {
    myKeyMapping->setTextColor(kTextColor);
    myKeyMapping->setText(instance().eventHandler().keyAtIndex(myActionSelected, myEventMode));
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void EventMappingWidget::enableButtons(bool state)
{
  myActionsList->setEnabled(state);
  myMapButton->setEnabled(state);
  myCancelMapButton->setEnabled(!state);
  myEraseButton->setEnabled(state);
  myResetButton->setEnabled(state);
  if(myComboButton)
  {
    Event::Type e =
      instance().eventHandler().eventAtIndex(myActionSelected, myEventMode);

    myComboButton->setEnabled(state && e >= Event::Combo1 && e <= Event::Combo16);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool EventMappingWidget::handleKeyDown(StellaKey key, StellaMod mod)
{
  // Remap keys in remap mode
  if (myRemapStatus && myActionSelected >= 0)
  {
    myKey = key;
    myMod |= mod;
  }
  return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool EventMappingWidget::handleKeyUp(StellaKey key, StellaMod mod)
{
  // Remap keys in remap mode
  if (myRemapStatus && myActionSelected >= 0
    && (mod & (KBDM_CTRL | KBDM_SHIFT | KBDM_ALT | KBDM_GUI)) == 0)
  {
    Event::Type event =
      instance().eventHandler().eventAtIndex(myActionSelected, myEventMode);
    if (instance().eventHandler().addKeyMapping(event, myEventMode, StellaKey(myKey), StellaMod(myMod)))
      stopRemapping();
  }
  return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void EventMappingWidget::handleJoyDown(int stick, int button)
{
  // Remap joystick buttons in remap mode
  if(myRemapStatus && myActionSelected >= 0)
  {
    Event::Type event =
      instance().eventHandler().eventAtIndex(myActionSelected, myEventMode);
    if(instance().eventHandler().addJoyButtonMapping(event, myEventMode, stick, button))
      stopRemapping();
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void EventMappingWidget::handleJoyAxis(int stick, int axis, int value)
{
  // Remap joystick axes in remap mode
  // There are two phases to detection:
  //   First, detect an axis 'on' event
  //   Then, detect the same axis 'off' event
  if(myRemapStatus && myActionSelected >= 0)
  {
    // Detect the first axis event that represents 'on'
    if(myLastStick == -1 && myLastAxis == -1 && value != 0)
    {
      myLastStick = stick;
      myLastAxis = axis;
      myLastValue = value;
    }
    // Detect the first axis event that matches a previously set
    // stick and axis, but turns the axis 'off'
    else if(myLastStick == stick && axis == myLastAxis && value == 0)
    {
      value = myLastValue;

      Event::Type event =
        instance().eventHandler().eventAtIndex(myActionSelected, myEventMode);
      if(instance().eventHandler().addJoyAxisMapping(event, myEventMode,
                                                     stick, axis, value))
        stopRemapping();
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool EventMappingWidget::handleJoyHat(int stick, int hat, JoyHat value)
{
  // Remap joystick hats in remap mode
  // There are two phases to detection:
  //   First, detect a hat direction event
  //   Then, detect the same hat 'center' event
  if(myRemapStatus && myActionSelected >= 0)
  {
    // Detect the first hat event that represents a valid direction
    if(myLastStick == -1 && myLastHat == -1 && value != JoyHat::CENTER)
    {
      myLastStick = stick;
      myLastHat = hat;
      myLastValue = int(value);

      return true;
    }
    // Detect the first hat event that matches a previously set
    // stick and hat, but centers the hat
    else if(myLastStick == stick && hat == myLastHat && value == JoyHat::CENTER)
    {
      value = JoyHat(myLastValue);

      Event::Type event =
        instance().eventHandler().eventAtIndex(myActionSelected, myEventMode);
      if(instance().eventHandler().addJoyHatMapping(event, myEventMode,
                                                    stick, hat, value))
      {
        stopRemapping();
        return true;
      }
    }
  }

  return false;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void EventMappingWidget::handleCommand(CommandSender* sender, int cmd,
                                       int data, int id)
{
  switch(cmd)
  {
    case ListWidget::kSelectionChangedCmd:
      if(myActionsList->getSelected() >= 0)
      {
        myActionSelected = myActionsList->getSelected();
        drawKeyMapping();
        enableButtons(true);
      }
      break;

    case ListWidget::kDoubleClickedCmd:
      if(myActionsList->getSelected() >= 0)
      {
        myActionSelected = myActionsList->getSelected();
        startRemapping();
      }
      break;

    case kStartMapCmd:
      startRemapping();
      break;

    case kStopMapCmd:
      stopRemapping();
      break;

    case kEraseCmd:
      eraseRemapping();
      break;

    case kResetCmd:
      resetRemapping();
      break;

    case kComboCmd:
      if(myComboDialog)
        myComboDialog->show(
          instance().eventHandler().eventAtIndex(myActionSelected, myEventMode),
          instance().eventHandler().actionAtIndex(myActionSelected, myEventMode));
      break;
  }
}
