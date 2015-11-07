--TEST--
Check for jsonrpc presence
--SKIPIF--
<?php if (!extension_loaded("jsonrpc")) print "skip"; ?>
--FILE--
<?php 
	$client = new Jsonrpc_Client();
    $client->call('http://192.168.80.140/jsonrpc-server.php', "addition", array(3,5));
	$client->call('http://192.168.80.140/jsonrpc-server.php', "addition", array(3,5));
    $client->call('http://192.168.80.140/jsonrpc-server.php', "addition", array(3,5));
    $client->execute();
?>
--EXPECT--
