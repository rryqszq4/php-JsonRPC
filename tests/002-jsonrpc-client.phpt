--TEST--
Check for jsonrpc presence
--SKIPIF--
<?php if (!extension_loaded("jsonrpc")) print "skip"; ?>
--FILE--
<?php 
	$client = new Jsonrpc_Client();
    $client->call('http://yaf-lib.com/rpc/json', "addition", array(3,5));
	$client->call('http://yaf-lib.com/rpc/json', "addition", array(3,5));
    $client->call('http://yaf-lib.com/rpc/json', "addition", array(3,5));
    $client->execute();
    var_dump($client);
?>
--EXPECT--
object(Jsonrpc_Client)#1 (1) {
  ["request"]=>
  object(Jsonrpc_Client_Request)#2 (0) {
  }
}