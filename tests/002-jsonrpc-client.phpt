--TEST--
Check for jsonrpc presence
--SKIPIF--
<?php if (!extension_loaded("jsonrpc")) print "skip"; ?>
--FILE--
<?php 
	$client = new Jsonrpc_Client();
    $client->call('http://www.baidu.com', "addition", array(3,5), "id_001");
    $client->call('http://www.baidu.com', "addition", array(3,5), "id_002");
    $client->call('http://www.baidu.com', "addition", array(3,5), "id_003");
    $result = $client->execute();
    echo $result[0]['id'];
    echo $result[1]['id'];
    echo $result[2]['id'];
?>
--EXPECT--
id_001id_002id_003