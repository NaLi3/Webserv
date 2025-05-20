<?php session_start(); ?>

<!DOCTYPE html>
<html>
    <head>
        <title>PHP Test - SESSION PAGE 1</title>
    </head>
    <body>
		<div>
        <?php
			if (isset($_SESSION["color"]))
			{
				$col = $_SESSION["color"];
			}
			else
			{
				$col = "black";
			}
			echo '<h1>PHP Session Page 1</h1>';
			echo '<p>This page allows to set session settings and see results.</p>';
			echo '<p>Current setting is :</p> <p style="color:' . $col . '">' . $col . '</p>';
		?>
		</div>
		<form action="/app/setsessionsetting.php" method="post">
			<p>Select setting :</p>
			<div>
				<input type="radio" id="color1" name="color" value="blue" />
				<label for="color1">BLUE</label>
				<input type="radio" id="color2" name="color" value="red" />
				<label for="color2">RED</label>
			</div>
			<div>
				<button type="submit">Submit</button>
			</div>
		</form>
		<div>
			<p>Go to <a href="/app/sessionpage2.php">Session page 2</a></p>
			<p><a href="/app/unsetsession.php">Unset session</a></p>
		</div>
    </body>
</html>