#pragma once
#include <stdbool.h>
typedef struct ClientData ClientData;
typedef struct {unsigned kind,id,cost,type;const char *name;} Product;
typedef struct {unsigned id,parent;const char *name;} SafeMessage;
ClientData *client_data_load(const char *path);
void client_data_free(ClientData *data);
bool client_product(const ClientData *data,unsigned kind,unsigned id,Product *product);
bool client_safe(const ClientData *data,unsigned id,SafeMessage *message);
unsigned client_safe_children(const ClientData *data,unsigned parent,unsigned offset,SafeMessage *out,unsigned capacity);
const char *client_tour(const ClientData *data,unsigned room);
const char *client_joke(const ClientData *data,unsigned index);
