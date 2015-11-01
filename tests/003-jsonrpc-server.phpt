--TEST--
Check for jsonrpc presence
--SKIPIF--
<?php if (!extension_loaded("jsonrpc")) print "skip"; ?>
--FILE--
<?php 
	$server = new Jsonrpc_Server();

// Procedures registration

        $server->register('addition', function ($a, $b) {
            return $a + $b;
        });

// Return the response to the client
        echo $server->execute();
?>
--EXPECT--
{"jsonrpc":"2.0","id":null,"error":{"code":-32700,"message":"Parse error"}}