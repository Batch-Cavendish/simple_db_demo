INSERT INTO users VALUES (1, 'Alice');
CREATE TABLE users (id INT, username TEXT);
INSERT INTO users VALUES (1, 'Alice');
INSERT INTO users VALUES (1, 'Duplicate');
DELETE FROM users WHERE id = 10;
UPDATE users SET username = 'NotThere' WHERE id = 10;
.exit
