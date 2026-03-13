CREATE TABLE users (id INT, username TEXT);
INSERT INTO users VALUES (1, 'This username is way too long for our 32 byte limit');
INSERT INTO users VALUES (1, 'Short');
UPDATE users SET username = 'This username is also way too long for our 32 byte limit' WHERE id = 1;
SELECT * FROM users;
.exit
