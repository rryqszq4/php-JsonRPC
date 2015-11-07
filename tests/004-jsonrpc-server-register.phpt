--TEST--
Check for jsonrpc presence
--SKIPIF--
<?php if (!extension_loaded("jsonrpc")) print "skip"; ?>
--FILE--
<?php 
	$payload = array(
		"jsonrpc"=>"2.0",
		"id"=>123456,
		"method"=>"addition",
		"params"=>array(2,3)
	);
	$server = new Jsonrpc_Server(json_encode($payload));

    /*
    $server->register('addition', function ($a, $b) {
        return $a + $b;
    });
    */

	/*
	$add = function ($a, $b){
		return $a + $b;
	};
    $server->register('addition', $add);
    */

    /*
    function add ($a, $b){
		return $a + $b;
	}
    $server->register('addition', 'add');
    */

    class A 
    {
    	static public function add($a, $b)
    	{
    		return $a + $b;
    	}
    }
    $server->register('addition', 'A::add');

    echo $server->execute();
?>
--EXPECT--
{"jsonrpc":"2.0","id":123456,"result":5}