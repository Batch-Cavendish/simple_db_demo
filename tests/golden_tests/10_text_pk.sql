CREATE TABLE users (username TEXT, id INT);
INSERT INTO users VALUES ('alice', 1);
INSERT INTO users VALUES ('bob', 2);
INSERT INTO users VALUES ('charlie', 3);
SELECT * FROM users WHERE username = 'alice';
SELECT * FROM users WHERE username = 'bob';
SELECT * FROM users WHERE username = 'eve';
.exit
