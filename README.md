JsonRPC 2.0 Client and Server
=============================

轻量级 JsonRPC 2.0 客户端和服务端的php扩展，基于 multi_curl + epoll的并发客户端，依据[jsonrpc](http://www.jsonrpc.org/)协议规范。
[en](https://github.com/rryqszq4/JsonRPC/blob/master/README-en.md)

环境
-----------
- PHP 5.3.*
- PHP 5.4.* 
- PHP 5.6.* 

安装
-------
```
$/path/to/phpize
$./configure --with-php-config=/path/to/php-config
$make && make install
```

例子
--------

### 服务端
server.php:

```php
<?php

$server = new Jsonrpc_Server();

// style one function variable
$add1 = function($a, $b){
	return $a + $b;
};
$server->register('addition1', $add1);

// style two function string
function add2($a, $b){
  return $a + $b;
}
$server->register('addition2', 'add2');

// style three function closure
$server->register('addition3', function ($a, $b) {
    return $a + $b;
});

//style four class method string
class A 
{
  static public function add($a, $b)
  {
    return $a + $b;
  }
}
$server->register('addition4', 'A::add');

echo $server->execute();

//output >>>
//{"jsonrpc":"2.0","id":null,"error":{"code":-32700,"message":"Parse error"}}

?>
```


### 客户端
client.php:

```php
<?php

$client = new Jsonrpc_Client(1);
$client->call('http://localhost/server.php', 'addition1', array(3,5));
$client->call('http://localhost/server.php', 'addition2', array(10,20));
$client->call('http://localhost/server.php', 'addition3', array(2,8));
$client->call('http://localhost/server.php', 'addition4', array(6,15));
/* ... */
$result = $client->execute();

var_dump($result);

//output >>>
/*
array(2) {
  [0]=>
  array(3) {
    ["jsonrpc"]=>
    string(3) "2.0"
    ["id"]=>
    int(110507766)
    ["result"]=>
    int(8)
  }
  [1]=>
  array(3) {
    ["jsonrpc"]=>
    string(3) "2.0"
    ["id"]=>
    int(1559316299)
    ["result"]=>
    int(30)
  }
  ...
}
*/
?>
```

####持久化
Jsonrpc_client(1) 参数为1的时候，将epoll资源进行持久化，默认使用非持久化。

####自定义 id
```php
<?php

$client = new Jsonrpc_client(1);
$client->call('http://localhost/server.php', 'addition', array(3,5),"custom_id_001");
$result = $client->execute();
var_dump($result);

//output >>>
/*
array(1) {
  [0]=>
  array(3) {
    ["jsonrpc"]=>
    string(3) "2.0"
    ["id"]=>
    string(13) "custom_id_001"
    ["result"]=>
    int(8)
  }
}
*/
?>
```

### 常见错误信息

|语法解析错误|{"jsonrpc":"2.0","id":null,"error":{"code":-32700,"message":"Parse error"}}|
|  无效请求  |{"jsonrpc":"2.0","id":null,"error":{"code":-32600,"message":"Invalid Request"}}
| 找不到方法 |{"jsonrpc":"2.0","id":null,"error":{"code":-32601,"message":"Method not found"}}|
| 无效的参数 |{"jsonrpc":"2.0","id":null,"error":{"code":-32602,"message":"Invalid params"}}|






