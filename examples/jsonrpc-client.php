<?php

$client = new Jsonrpc_Client();
$client->call("http://yaf-lib.com/", "addition", array(3,5));
?>
