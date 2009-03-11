/* varmessage.h - several variations on messages */

#ifndef varmessage
#define varmessage

#include <IV-2_6/InterViews/message.h>
#include <mymessage.h>  // ivfasd/mymessage.h

#include <IV-2_6/_enter.h>

/*
    In order to make these do anything you need to include the
    following (or make your own) entries in properties[] ...

static PropertyData properties[] = {
    { "*GenTitleMessage*font",		"*times-bold-r-normal--24*" },
    { "*GenTitleItalMessage*font",	"*times-medium-i-normal--24*" },
    { "*GenTitleItalMenu*font",		"*times-medium-i-normal--14*" },
    { "*BoldMessage*font",		"*helvetica-bold-r-normal--12*" },
    { "*BoldMyMessage*font",		"*helvetica-bold-r-normal--12*" },
    { "*BigBoldMessage*font",		"*helvetica-bold-r-normal--18*" },
    { "*BigBoldMyMessage*font",		"*helvetica-bold-r-normal--18*" },
}
*/

class GenTitleMessage;
class GenTitleItalMessage;
class GenTitleItalMenu;

class BoldMessage;
class BoldMyMessage;

class BigBoldMessage;
class BigBoldMyMessage;

/*****************************************************************************/

class GenTitleMessage : public Message {
 public:
  GenTitleMessage(const char*, Alignment = Left);
};

inline GenTitleMessage::GenTitleMessage(const char * c, Alignment a) 
	: Message(c, a)
{
  SetClassName("GenTitleMessage");
}

/*****************************************************************************/

class GenTitleItalMessage : public Message {
 public:
  GenTitleItalMessage(const char*, Alignment = Left);
};

inline GenTitleItalMessage::GenTitleItalMessage(const char * c, Alignment a) 
	: Message(c, a)
{
  SetClassName("GenTitleItalMessage");
};

/*****************************************************************************/

class GenTitleItalMenu : public Message {
 public:
  GenTitleItalMenu(const char*, Alignment = Left);
};

inline GenTitleItalMenu::GenTitleItalMenu(const char * c, Alignment a) 
	: Message(c, a)
{
  SetClassName("GenTitleItalMenu");
};

/*****************************************************************************/

class BoldMessage : public Message {
 public:
  BoldMessage(const char*, Alignment = Left);
};

inline BoldMessage::BoldMessage(const char * c, Alignment a) 
	: Message(c, a)
{
  SetClassName("BoldMessage");
};

/*****************************************************************************/

class BoldMyMessage : public MyMessage {
 public:
  BoldMyMessage(char*, Alignment = Center);
};

inline BoldMyMessage::BoldMyMessage(char * c, Alignment a) 
	: MyMessage(c, a)
{
  SetClassName("BoldMyMessage");
};

/*****************************************************************************/

class BigBoldMessage : public Message {
 public:
  BigBoldMessage(const char*, Alignment = Left);
};

inline BigBoldMessage::BigBoldMessage(const char * c, Alignment a) 
	: Message(c, a)
{
  SetClassName("BigBoldMessage");
}

/*****************************************************************************/

class BigBoldMyMessage : public MyMessage {
 public:
  BigBoldMyMessage(char*, Alignment = Center);
};

inline BigBoldMyMessage::BigBoldMyMessage(char * c, Alignment a) 
	: MyMessage(c, a)
{
  SetClassName("BigBoldMyMessage");
}

#include <IV-2_6/_leave.h>

#endif
