<!DOCTYPE html>
<html>
    <head>
        <title>PHP Test</title>
    </head>
    <body>
        <?php echo '<h1>PHP wants to sleep</h1>';

			echo "<p>Hour is " . date("h:i:sa") . "</p>";
			echo "<p>PHP goes to sleep for 10 seconds</p>";
			sleep(10);
			echo "<p>PHP wakes up</p>";
			echo "<p>Hour is " . date("h:i:sa") . "</p>";

		?>
    </body>
</html>