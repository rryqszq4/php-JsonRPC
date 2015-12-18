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
    $res = $client->execute();
    var_dump($res);
?>
--EXPECT--
array(3) {
  [0]=>
  array(3) {
    ["jsonrpc"]=>
    string(3) "2.0"
    ["id"]=>
    string(6) "id_001"
    ["error"]=>
    array(2) {
      ["code"]=>
      int(-32302)
      ["message"]=>
      string(16) "Move temporarily"
    }
  }
  [1]=>
  array(3) {
    ["jsonrpc"]=>
    string(3) "2.0"
    ["id"]=>
    string(6) "id_002"
    ["error"]=>
    array(2) {
      ["code"]=>
      int(-32302)
      ["message"]=>
      string(16) "Move temporarily"
    }
  }
  [2]=>
  array(3) {
    ["jsonrpc"]=>
    string(3) "2.0"
    ["id"]=>
    string(6) "id_003"
    ["error"]=>
    array(2) {
      ["code"]=>
      int(-32302)
      ["message"]=>
      string(16) "Move temporarily"
    }
  }
}