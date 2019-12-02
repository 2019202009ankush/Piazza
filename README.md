1. Connection Establish
	{
		"type":"client/slave"
	}

2. Message Format for Create User
	{
		"purpose":"create_user",
		"username":"",
		"password":""
	}

3. Login
	{
		"purpose":"login",
		"username":"",
		"password":""
	}
	
4. Logout
	{
		"purpose":"logout",
		"username":""
	}
	
5. Get
	{
		"purpose":"get",
		"key":""
	}
	
6. Get Response
	{
		"value":"If value exist then value else failure"
	}

7. Create_user Response
	{
		"value":"exists/success"
	}

8. Delete
	{
		"purpose":"delete",
		"key":""
	}

9. Delete Response
	{
		"purpose":"delete",
		"value":"success/failure/nexists"
	}

10. Put
	{
		"purpose":"put",
		"key":"",
		"value":""
	}
	
	Put Response
	{
		"purpose":"put"
		"response":"Success/Exists"
	}

11. Update
	{
		"purpose":"update",
		"key":"",
		"value":""
	}

	Update Response 
	{
		"purpose":"update",
		"value":"exists/added/failure"
	}

12. Value in Put/Update (Nested JSON String)
	{
		"col_name":"col_value",
		"col_name":"col_value",
		.
		.
		.
	}

13. Put Response
	{
		"value":"success/exists/failure"
	}
	
14. Update Response
	{
		"value":"success/failure"
	}
	
15. HeartBeat
	{
		"purpose":"heartbeat"
	}

16. Connection Termination
	{
		"purpose":"termination"
	}
	
17. Migration
	msg1: From coor. to slave
	{
		"purpose":"migration",
		"task":"get",
		"ip":"",
		"port":"",
		"what":"prev/curr",
		"addTo":"prev/curr"
	}
	msg2: From slave to slave
	{
		"purpose":"migration",
		"task":"send",
		"what":"prev/curr"
	}
	msg3: From coord. to slave
	{
		"purpose":"migration",
		"task":"merge"
	}
