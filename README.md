# TinyNet
借用了@skynet的线程模型，一个IO线程负责监听，连接，发送和接收，把网络事件分发到若干个业务逻辑线程处理，业务逻辑线程使用双缓冲队列，保证同一个连接上的数据包有序处理。

需完善的地方

1 业务逻辑分发处理没有删除不在需要的数据项的业务。最近打算修复，记录连接成功调用次数和断开连接次数，发现次数匹配后即删除

2 IO线程终止方式需修改

3 代码测试不够完善,可能仍遗留有大量BUG

==========================================================================================================================

dispatcher修改了1，如果某sockethandler没有在使用，立即删除。同时把serverhandler单独处理，不再和sockethandler放在一起了。

socketmanager修改了2，线程退出前关闭所有套接字，同时等待所有异步操作返回，可能依然有问题。

添加了scheduler，增加了定时器功能

以及修改了其他bug。
