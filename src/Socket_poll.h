int put_event2(engine_t e,st_io *io);
void on_read_active(socket_t s)
{
	assert(s);
	spin_lock(s->recv_mtx);
	s->readable = 1;
	int active_recv_count = ++s->active_read_count;
	st_io *req = LINK_LIST_POP(st_io*,s->pending_recv);
	spin_unlock(s->recv_mtx);
	
	if(req)
		put_event2(s->engine,req);
}

void on_write_active(socket_t s)
{
	assert(s);
	spin_lock(s->send_mtx);
	s->writeable = 1;
	int active_send_count = ++s->active_write_count;
	st_io *req = LINK_LIST_POP(st_io*,s->pending_send);
	spin_unlock(s->send_mtx);
	if(req)
		put_event2(s->engine,req);
}

int _recv(socket_t s,st_io* io_req,int active_recv_count)
{
	assert(s);assert(io_req);
	while(1)
	{
		int retry = 0;
		int bytes_transfer = io_req->bytes_transfer = TEMP_FAILURE_RETRY(readv(s->fd,io_req->iovec,io_req->iovec_count));
		io_req->error_code = 0;
		if(bytes_transfer < 0)
		{
			switch(errno)
			{
				case EAGAIN:
				{
					spin_lock(s->recv_mtx);
					if(active_recv_count != s->active_read_count)
					{
						active_recv_count = s->active_read_count;
						retry = 1;
					}
					else
					{
						s->readable = 0;
						LINK_LIST_PUSH_FRONT(s->pending_recv,io_req);
					}
					spin_unlock(s->recv_mtx);
					
					if(retry)
						continue;
					return 0;
				}
				break;
				default://Á¬œÓ³öŽí
				{
					io_req->error_code = errno;
				}
				break;
			}
		}
		else if(bytes_transfer == 0)
			bytes_transfer = -1;
		return bytes_transfer;	
	}	
}

int _send(socket_t s,st_io* io_req,int active_send_count)
{
	assert(s);assert(io_req);
	while(1)
	{
		int retry = 0;
		int bytes_transfer = io_req->bytes_transfer = TEMP_FAILURE_RETRY(writev(s->fd,io_req->iovec,io_req->iovec_count));
		io_req->error_code = 0;	
		if(bytes_transfer < 0)
		{
			switch(errno)
			{
				case EAGAIN:
				{
					spin_lock(s->send_mtx);
					if(active_send_count != s->active_write_count)
					{
						active_send_count = s->active_write_count;
						retry = 1;
					}
					else
					{
						s->writeable = 0;
						//œ«ÇëÇóÖØÐÂ·Å»Øµœ¶ÓÁÐ
						LINK_LIST_PUSH_FRONT(s->pending_send,io_req);
					}
					spin_unlock(s->send_mtx);
					
					if(retry)
						continue;
					return 0;
				}
				break;
				default://Á¬œÓ³öŽí
				{
					io_req->error_code = errno;
				}
				break;
			}
		}
		else if(bytes_transfer == 0)
			bytes_transfer = -1;	
		return bytes_transfer;	
	}
}
