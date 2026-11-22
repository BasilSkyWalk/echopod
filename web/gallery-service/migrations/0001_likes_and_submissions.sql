CREATE TABLE IF NOT EXISTS likes (
  theme TEXT NOT NULL,
  voter TEXT NOT NULL,
  address TEXT NOT NULL,
  day TEXT NOT NULL,
  PRIMARY KEY (theme, voter)
);

CREATE INDEX IF NOT EXISTS likes_by_address ON likes (address, day);

CREATE TABLE IF NOT EXISTS counts (
  theme TEXT PRIMARY KEY,
  likes INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS submissions (
  address TEXT NOT NULL,
  day TEXT NOT NULL,
  file TEXT NOT NULL,
  pull INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS submissions_by_address ON submissions (address, day);
