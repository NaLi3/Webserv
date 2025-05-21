<?php session_start() ?>
<!DOCTYPE html>
<html>
    <head>
        <title>PHP Test</title>
    </head>
    <body>
        <?php echo '<h1>PHP says hello</h1>';
		$_SESSION["foo"] = "bar";

			echo "<p>This is a dynamically generated page : Hour is " . date("h:i:sa") . "</p>";

		?>
    </body>
</html>