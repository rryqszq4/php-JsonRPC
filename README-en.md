JsonRPC 2.0 Client and Server
=============================

Lightweight multi Json-RPC 2.0 client/server in php extension

Requirement
-----------
- PHP 5.3 +

Install
-------
```
$/path/to/phpize
$./configure --with-php-config=/path/to/php-config
$make && make install
```

Examples
--------

### Server
Callback binding:

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


### Client
Example with positional parameters:

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

client custom id
```php
<?php

$client = new Jsonrpc_client(1);
$client->call('http://192.168.80.140/jsonrpc-server.php', 'addition', array(3,5),"custom_id_001");
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

### Error INFO
Parse error
```
{"jsonrpc":"2.0","id":null,"error":{"code":-32700,"message":"Parse error"}}
```

Invalid Request
```
{"jsonrpc":"2.0","id":null,"error":{"code":-32600,"message":"Invalid Request"}}
```

Method not found
```
{"jsonrpc":"2.0","id":null,"error":{"code":-32601,"message":"Method not found"}}
```

Invalid params
```
{"jsonrpc":"2.0","id":null,"error":{"code":-32602,"message":"Invalid params"}}
```

