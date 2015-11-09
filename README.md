JsonRPC 2.0 Client and Server
=============================

轻量级 Json-RPC 2.0 客户端和服务端的php扩展，基于libcurl + epoll的并发客户端，依据jsonrpc[http://www.jsonrpc.org/]协议规范。
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

$client = new Jsonrpc_Client();
$client->call('http://localhost/server.php', 'addition', array(3,5));
$client->call('http://localhost/server.php', 'addition', array(10,20));
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

```

