# 定制Lua

## 异常处理

调试器支持两种异常，未捕获的异常和已捕获的异常。对于未捕获的异常，调试器依赖`lua_panic`来实现。已捕获的异常lua本身没有提供足够好的支持。所以调试器提供了两种方案来让你帮助调试器支持已捕获的异常。

> 在合适的时机，调用debugger::exception。

调用debugger::exception之后，会立刻触发异常断点，并停在此处。所以一般在lua_pcall的错误函数里调用它即可。

debugger::exception参数以及输入

* 在lua栈顶存放当前异常的文本(string)。
* L 有异常的lua线程
* exceptionType 目前只支持固定的两个类型caught和uncaught。由于目前vscode没有支持CapabilitiesEvent，所以无法自定义异常类型。
* level 从第几层开始做栈回溯

> 给你的lua打补丁

``` patch
ldblib.c:
@@ -326,6 +326,7 @@
   if (strchr(smask, 'c')) mask |= LUA_MASKCALL;
   if (strchr(smask, 'r')) mask |= LUA_MASKRET;
   if (strchr(smask, 'l')) mask |= LUA_MASKLINE;
+  if (strchr(smask, 'e')) mask |= LUA_MASKEXCEPTION;
   if (count > 0) mask |= LUA_MASKCOUNT;
   return mask;
 }
@@ -339,6 +340,7 @@
   if (mask & LUA_MASKCALL) smask[i++] = 'c';
   if (mask & LUA_MASKRET) smask[i++] = 'r';
   if (mask & LUA_MASKLINE) smask[i++] = 'l';
+  if (mask & LUA_MASKEXCEPTION) smask[i++] = 'e';
   smask[i] = '\0';
   return smask;
 }
ldebug.c:
@@ -638,6 +638,8 @@
 
 
 l_noret luaG_errormsg (lua_State *L) {
+  if (L->hookmask & LUA_MASKEXCEPTION)
+    luaD_hook(L, LUA_HOOKEXCEPTION, -1);
   if (L->errfunc != 0) {  /* is there an error handling function? */
     StkId errfunc = restorestack(L, L->errfunc);
     setobjs2s(L, L->top, L->top - 1);  /* move argument */
lua.h:
@@ -404,15 +404,17 @@
 #define LUA_HOOKLINE	2
 #define LUA_HOOKCOUNT	3
 #define LUA_HOOKTAILCALL 4
+#define LUA_HOOKEXCEPTION 5
 
 
 /*
 ** Event masks
 */
 #define LUA_MASKCALL	(1 << LUA_HOOKCALL)
 #define LUA_MASKRET	(1 << LUA_HOOKRET)
 #define LUA_MASKLINE	(1 << LUA_HOOKLINE)
 #define LUA_MASKCOUNT	(1 << LUA_HOOKCOUNT)
+#define LUA_MASKEXCEPTION	(1 << LUA_HOOKEXCEPTION)
 
 typedef struct lua_Debug lua_Debug;  /* activation record */
 
```

## 函数原型

调试器为了加速某些操作，需要对部分调试信息进行缓存。但lua的官方实现中没有快速的办法在调试钩子中得知lua当前所在的位置。一种可行的方案是使用`lua_getinfo(L, "f", &debug)`，它确实足够的快，唯一的问题是函数在运行的过程中会不断地变多，也许给缓存加一个过期时间也能解决这个问题。

更好的解决办法是使用函数原型来索引缓存。你可以给你的lua打上这个补丁

``` patch
ldebug.c:
@@ -697,9 +697,10 @@
   }
 }
 
+const void* lua_getproto(lua_State *L, int idx) {
+	const LClosure *c;
+	if (!lua_isfunction(L, idx) || lua_iscfunction(L, idx))
+		return 0;
+	c = lua_topointer(L, idx);
+	return c->p;
+}
lua.h:
@@ -437,6 +437,8 @@
 LUA_API int (lua_gethookmask) (lua_State *L);
 LUA_API int (lua_gethookcount) (lua_State *L);
 
+LUA_API const void* (lua_getproto)(lua_State *L, int idx);
+
 
 struct lua_Debug {
   int event;
```

当然你不打补丁，调试器也会使用相同的代码来获取函数原型。但如果你修改过你的lua，比如修过了LClosure的定义。那么调试器的代码就可能有问题，这时你就必须打上补丁。
