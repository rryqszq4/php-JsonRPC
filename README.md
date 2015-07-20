JsonRPC 2.0 Client and Server
=============================

Lightweight Json-RPC 2.0 client/server in php extension

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

// Procedures registration

$server->register('addition', function ($a, $b) {
    return $a + $b;
});

// Return the response to the client
echo $server->execute();

?>
```

### Client
Example with positional parameters:

```php
<?php

$client = new Jsonrpc_Client('http://localhost/server.php');
$result = $client->execute('addition', array(3,5));

var_dump($result);
```

