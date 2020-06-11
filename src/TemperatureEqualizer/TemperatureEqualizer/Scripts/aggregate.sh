#!/bin/bash
rm /home/postgres/aggregate_data.log
psql -f /home/postgres/aggregate_data.sql -1 -L /home/postgres/aggregate_data.log -d thingsboard
