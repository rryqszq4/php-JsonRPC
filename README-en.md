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
?>
```

