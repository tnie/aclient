#pragma once
#include "AClient.h"
class WebSocketRequest :
    public HTTPRequest
{
public:
    WebSocketRequest();
    ~WebSocketRequest();
};

