--TEST--
Check for jsonrpc presence
--SKIPIF--
<?php if (!extension_loaded("jsonrpc")) print "skip"; ?>
--FILE--
<?php 
	$client = new Jsonrpc_Client();
    $client->call('http://www.baidu.com', "addition", array(3,5));
	$client->call('http://www.baidu.com', "addition", array(3,5));
	$client->call('http://www.baidu.com', "addition", array(3,5));
    $client->execute();
?>
--EXPECT--
